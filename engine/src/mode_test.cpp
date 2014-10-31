/* Copyright (C) 2003-2013 Runtime Revolution Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"

#include "execpt.h"
#include "scriptpt.h"
#include "dispatch.h"
#include "stack.h"
#include "tooltip.h"
#include "card.h"
#include "field.h"
#include "button.h"
#include "image.h"
#include "aclip.h"
#include "vclip.h"
#include "stacklst.h"
#include "mcerror.h"
#include "hc.h"
#include "util.h"
#include "param.h"
#include "debug.h"
#include "statemnt.h"
#include "funcs.h"
#include "magnify.h"
#include "sellst.h"
#include "undolst.h"
#include "styledtext.h"
#include "property.h"
#include "internal.h"
#include "globals.h"
#include "license.h"
#include "mode.h"
#include "revbuild.h"
#include "deploy.h"
#include "capsule.h"
#include "player.h"
#include "osspec.h"
#include "stacksecurity.h"

#include "test.h"

////////////////////////////////////////////////////////////////////////////////
//
//  Globals specific to TEST mode
//

struct MCCapsuleInfo
{
	uint32_t size;
	uint32_t data[3];
};

// Define the needed sections on a platform-by-platform basis.
#if defined(_WINDOWS)

#define PAYLOAD_SECTION_NAME ".payload"
#define PROJECT_SECTION_NAME ".project"

#pragma section(".payload", read, discard)
__declspec(allocate(".payload")) volatile MCCapsuleInfo MCpayload = {};
#pragma section(".project", read, discard)
__declspec(allocate(".project")) volatile MCCapsuleInfo MCcapsule = {};

#elif defined(_LINUX)

#define PAYLOAD_SECTION_NAME ".payload"
#define PROJECT_SECTION_NAME ".project"

__attribute__((section(".payload"))) volatile MCCapsuleInfo MCpayload = {};
__attribute__((section(".project"))) volatile MCCapsuleInfo MCcapsule = {};

#elif defined(_MACOSX)

#define PAYLOAD_SECTION_NAME "__PAYLOAD"
#define PROJECT_SECTION_NAME "__PROJECT"

__attribute__((section("__PROJECT,__project"))) volatile MCCapsuleInfo MCcapsule = {};

#endif

// The default license parameters to use for the installer engine.
MCLicenseParameters MClicenseparameters =
{
	NULL, NULL, NULL, kMCLicenseClassNone, 0,
	10, 10, 50, 10,
	0,
	NULL,
};

// This method is implemented per-platform and locates a named section in the 
// current exe.
static void *MCExecutableFindSection(const char *p_name);

// We don't include error string in this mode
const char *MCparsingerrors = "";
const char *MCexecutionerrors = "";

////////////////////////////////////////////////////////////////////////////////

// Small helper template
template<class T> inline MCStatement *class_factory(void) { return new T; }

// The internal verb table used by the '_internal' command
MCInternalVerbInfo MCinternalverbs[] =
{
	{ nil, nil, nil }
};

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of MCDispatch::startup method for TEST mode.
//

int MChighleveltestcount = 0;
int MChighleveltestindex = 0;

void MCDispatch::timer(MCNameRef p_name, MCParameter *p_parameters)
{
    if (!MCNameIsEqualTo(p_name, MCM_internal, kMCCompareCaseless))
        return;
    
    MCExecuteHighLevelTest(MChighleveltestindex);
    
    MChighleveltestindex += 1;
    
    if (MChighleveltestindex < MChighleveltestcount)
        MCscreen -> addtimer(this, MCM_internal, 0);
}

IO_stat MCDispatch::startup(void)
{
	startdir = MCS_getcurdir();
	enginedir = strclone(MCcmd);
	char *eptr = strrchr(enginedir, PATH_SEPARATOR);
	if (eptr != NULL)
		*eptr = '\0';
	else
		*enginedir = '\0';
	char *openpath = MCcmd; //point to MCcmd string

	// set up image cache before the first stack is opened
	MCCachedImageRep::init();

	// Get the list of all low-level tests, and execute one by one.
    int t_lowlevel_test_count;
    t_lowlevel_test_count = MCCountLowLevelTests();
    for(int i = 0; i < t_lowlevel_test_count; i++)
        MCExecuteLowLevelTest(i);
	
    // Now start a 'timer' message loop, executing each high-level test in turn.
    MChighleveltestcount = MCCountHighLevelTests();
    MChighleveltestindex = 0;
    if (MChighleveltestcount > 0)
        MCscreen -> addtimer(this, MCM_internal, 0);
    
    MCStack *t_stack;
    MCStackSecurityCreateStack(t_stack);
    t_stack -> setparent(this);
	stacks = t_stack;
    
	MCcmd = openpath;
	MCdefaultstackptr = MCstaticdefaultstackptr = t_stack;

	t_stack -> extraopen(false);

	MCscreen->resetcursors();
    MCImage::init();

	return IO_NORMAL;
}

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of MCStack::mode* hooks for TEST mode.
//

Exec_stat MCStack::mode_getprop(uint4 parid, Properties which, MCExecPoint &ep, const MCString &carray, Boolean effective)
{
	return ES_NOT_HANDLED;
}

Exec_stat MCStack::mode_setprop(uint4 parid, Properties which, MCExecPoint &ep, const MCString &cprop, const MCString &carray, Boolean effective)
{
	return ES_NOT_HANDLED;
}

void MCStack::mode_load(void)
{
}

void MCStack::mode_getrealrect(MCRectangle& r_rect)
{
	MCscreen->getwindowgeometry(window, r_rect);
}

void MCStack::mode_takewindow(MCStack *other)
{
}

void MCStack::mode_takefocus(void)
{
	MCscreen->setinputfocus(window);
}

char *MCStack::mode_resolve_filename(const char *filename)
{
	return NULL;
}

bool MCStack::mode_needstoopen(void)
{
	return true;
}

void MCStack::mode_setgeom(void)
{
}

void MCStack::mode_setcursor(void)
{
	MCscreen->setcursor(window, cursor);
}

bool MCStack::mode_openasdialog(void)
{
	return true;
}

void MCStack::mode_closeasdialog(void)
{
}

void MCStack::mode_openasmenu(MCStack *grab)
{
}

void MCStack::mode_closeasmenu(void)
{
}

void MCStack::mode_constrain(MCRectangle& rect)
{
}

#ifdef _WINDOWS
MCSysWindowHandle MCStack::getrealwindow(void)
{
	return window->handle.window;
}

MCSysWindowHandle MCStack::getqtwindow(void)
{
	return window->handle.window;
}

#endif

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of MCObject::mode_get/setprop for STANDALONE mode.
//

Exec_stat MCObject::mode_getprop(uint4 parid, Properties which, MCExecPoint &ep, const MCString &carray, Boolean effective)
{
	return ES_NOT_HANDLED;
}

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of MCProperty::mode_eval/mode_set for INSTALLER mode.
//

Exec_stat MCProperty::mode_set(MCExecPoint& ep)
{
	return ES_NOT_HANDLED;
}

Exec_stat MCProperty::mode_eval(MCExecPoint& ep)
{
	return ES_NOT_HANDLED;
}

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of mode hooks for INSTALLER mode.
//

// In standalone mode, the standalone stack built into the engine cannot
// be saved.
IO_stat MCModeCheckSaveStack(MCStack *sptr, const MCString& filename)
{
	if (sptr == MCdispatcher -> getstacks())
	{
		MCresult->sets("can't save into test");
		return IO_ERROR;
	}

	return IO_NORMAL;
}

// In standalone mode, the environment depends on various command-line/runtime
// globals.
const char *MCModeGetEnvironment(void)
{
	return "installer";
}

uint32_t MCModeGetEnvironmentType(void)
{
	return kMCModeEnvironmentTypeInstaller;
}

// In standalone mode, we are never licensed.
bool MCModeGetLicensed(void)
{
	return false;
}

// In standalone mode, the executable is $0 if there is an embedded stack.
bool MCModeIsExecutableFirstArgument(void)
{
	return true;
}

// In standalone mode, we only automatically open stacks if there isn't an
// embedded stack.
bool MCModeShouldLoadStacksOnStartup(void)
{
	return false;
}

// In standalone mode, we try to work out what went wrong...
void MCModeGetStartupErrorMessage(const char*& r_caption, const char *& r_text)
{
	r_caption = "Initialization Error";
	if (MCresult -> getvalue() . is_string())
		r_text = MCresult -> getvalue() . get_string() . clone();
	else
		r_text = "unknown reason";
}

// In standalone mode, we can only set an object's script if has non-zero id.
bool MCModeCanSetObjectScript(uint4 obj_id)
{
	return obj_id != 0;
}

// In standalone mode, we must check the old CANT_STANDALONE flag.
bool MCModeShouldCheckCantStandalone(void)
{
	return true;
}

// The standalone mode doesn't have a message box redirect feature
bool MCModeHandleMessageBoxChanged(MCExecPoint& ep)
{
	return false;
}

// The standalone mode causes a relaunch message.
bool MCModeHandleRelaunch(const char *& r_id)
{
#ifdef _WINDOWS
	bool t_do_relaunch;
	const char *t_id;

	t_do_relaunch = MCdefaultstackptr -> hashandler(HT_MESSAGE, MCM_relaunch) == True;
	t_id = MCdefaultstackptr -> getname_cstring();
	r_id = t_id;

	return t_do_relaunch;
#else
	return false;
#endif
}

// The standalone mode's startup stack depends on whether it has a stack embedded.
const char *MCModeGetStartupStack(void)
{
	return NULL;
}

bool MCModeCanLoadHome(void)
{
	return false;
}

MCStatement *MCModeNewCommand(int2 which)
{
	switch(which)
	{
	case S_INTERNAL:
		return new MCInternal;
	default:
		break;
	}

	return NULL;
}

MCExpression *MCModeNewFunction(int2 which)
{
	return NULL;
}

void MCModeObjectDestroyed(MCObject *object)
{
}

MCObject *MCModeGetU3MessageTarget(void)
{
	return MCdefaultstackptr -> getcard();
}

bool MCModeShouldQueueOpeningStacks(void)
{
	return MCscreen == NULL;
}

bool MCModeShouldPreprocessOpeningStacks(void)
{
	return false;
}

Window MCModeGetParentWindow(void)
{
	Window t_window;
	t_window = MCdefaultstackptr -> getwindow();
	if (t_window == NULL && MCtopstackptr != NULL)
		t_window = MCtopstackptr -> getwindow();
	return t_window;
}

bool MCModeCanAccessDomain(const char *p_name)
{
	return false;
}

void MCModeQueueEvents(void)
{
}

Exec_stat MCModeExecuteScriptInBrowser(const MCString& script)
{
	MCeerror -> add(EE_ENVDO_NOTSUPPORTED, 0, 0);
	return ES_ERROR;
}

bool MCModeMakeLocalWindows(void)
{
	return true;
}

void MCModeActivateIme(MCStack *p_stack, bool p_activate)
{
	MCscreen -> activateIME(p_activate);
}

void MCModeConfigureIme(MCStack *p_stack, bool p_enabled, int32_t x, int32_t y)
{
	if (!p_enabled)
		MCscreen -> clearIME(p_stack -> getwindow());
}

void MCModeShowToolTip(int32_t x, int32_t y, uint32_t text_size, uint32_t bg_color, const char *text_font, const char *message)
{
}

void MCModeHideToolTip(void)
{
}

void MCModeResetCursors(void)
{
	MCscreen -> resetcursors();
}

bool MCModeCollectEntropy(void)
{
	return false;
}

// We return false here as at present, in installers, the first stack does not
// sit in the message path of all stacks.
bool MCModeHasHomeStack(void)
{
	return false;
}

// IM-2014-08-08: [[ Bug 12372 ]] Pixel scaling is enabled for the installer
bool MCModeGetPixelScalingEnabled(void)
{
	return true;
}

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of remote dialog methods
//

void MCRemoteFileDialog(MCExecPoint& ep, const char *p_title, const char *p_prompt, const char * const p_types[], uint32_t p_type_count, const char *p_initial_folder, const char *p_initial_file, bool p_save, bool p_files)
{
}

void MCRemoteColorDialog(MCExecPoint& ep, const char *p_title, uint32_t p_red, uint32_t p_green, uint32_t p_blue)
{
}

void MCRemoteFolderDialog(MCExecPoint& ep, const char *p_title, const char *p_prompt, const char *p_initial)
{
}

void MCRemotePrintSetupDialog(char *&r_reply_data, uint32_t &r_reply_data_size, uint32_t &r_result, const char *p_config_data, uint32_t p_config_data_size)
{
}

void MCRemotePageSetupDialog(char *&r_reply_data, uint32_t &r_reply_data_size, uint32_t &r_result, const char *p_config_data, uint32_t p_config_data_size)
{
}

#ifdef _MACOSX
uint32_t MCModePopUpMenu(MCMacSysMenuHandle p_menu, int32_t p_x, int32_t p_y, uint32_t p_index, MCStack *p_stack)
{
	return 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of Windows-specific mode hooks for STANDALONE mode.
//

#ifdef TARGET_PLATFORM_WINDOWS

typedef BOOL (WINAPI *AttachConsolePtr)(DWORD id);
void MCModePreMain(void)
{
	HMODULE t_kernel;
	t_kernel = LoadLibraryA("kernel32.dll");
	if (t_kernel != nil)
	{
		void *t_attach_console;
		t_attach_console = GetProcAddress(t_kernel, "AttachConsole");
		if (t_attach_console != nil)
		{
			((AttachConsolePtr)t_attach_console)(-1);
			return;
		}
	}
}

void MCModeSetupCrashReporting(void)
{
}

bool MCModeHandleMessage(LPARAM lparam)
{
	return false;
}

bool MCPlayer::mode_avi_closewindowonplaystop()
{
	return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of Mac OS X-specific mode hooks for DEVELOPMENT mode.
//

#ifdef _MACOSX

bool MCModePreWaitNextEvent(Boolean anyevent)
{
	return false;
}

#endif

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of Linux-specific mode hooks for DEVELOPMENT mode.
//

#ifdef _LINUX

void MCModePreSelectHook(int& maxfd, fd_set& rfds, fd_set& wfds, fd_set& efds)
{
}

void MCModePostSelectHook(fd_set& rfds, fd_set& wfds, fd_set& efds)
{
}

#endif

////////////////////////////////////////////////////////////////////////////////
//
//  Implementation of the per-platform section location functions
//

#if defined(_WINDOWS)
static void *MCExecutableFindSection(const char *p_name)
{
	// The 'module handle' is nothing more than the base address of the PE image
	void *t_base;
	t_base = GetModuleHandle(NULL);

	// PE images start with a DOS_HEADER
	IMAGE_DOS_HEADER *t_dos_header;
	t_dos_header = (IMAGE_DOS_HEADER *)t_base;

	// The main (NT) header is located based on an offset in the DOS header
	IMAGE_NT_HEADERS *t_nt_headers;
	t_nt_headers = (IMAGE_NT_HEADERS *)((char *)t_base + t_dos_header -> e_lfanew);

	// The list of sections is after the 'OPTIONAL_HEADER', this macro computes
	// the address for us.
	IMAGE_SECTION_HEADER *t_sections;
	t_sections = IMAGE_FIRST_SECTION(t_nt_headers);

	// Now just loop through until we find the section we are looking for. Note
	// that the 'VirtualAddress' field is relative to ImageBase - hence the
	// adjustment.
	for(uint32_t i = 0; i < t_nt_headers -> FileHeader . NumberOfSections; i++)
		if (memcmp(t_sections[i] . Name, p_name, MCU_min((unsigned)IMAGE_SIZEOF_SHORT_NAME, strlen(p_name))) == 0)
			return (void *)(t_sections[i] . VirtualAddress + t_nt_headers -> OptionalHeader . ImageBase);

	// We didn't find the section - oh dear.
	return NULL;
}
#elif defined(_MACOSX)
#include <mach-o/dyld.h>

static void *MCExecutableFindSection(const char *p_name)
{
	const mach_header *t_header;
	t_header = _dyld_get_image_header(0);
	
	const load_command *t_command;
	t_command = (const load_command *)(t_header + 1);
	
	for(uint32_t i = 0; i < t_header -> ncmds; i++)
	{
		if (t_command -> cmd == LC_SEGMENT)
		{
			const segment_command *t_segment;
			t_segment = (const segment_command *)t_command;
			
			if (MCMemoryEqual(t_segment -> segname, p_name, MCMin(16, strlen(p_name) + 1)))
			{
				const section *t_section;
				t_section = (const section *)(t_segment + 1);
				return (void *)t_section -> addr;
			}
		}
		
		t_command = (const load_command *)((uint8_t *)t_command + t_command -> cmdsize);
	}
	
	return NULL;
}
#elif defined(_LINUX)
#include <elf.h>

// MW-2013-05-03: [[ Linux64 ]] Make sure we use the correct structs for section
//   searching.

#ifdef __LP64__
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Phdr Elf_Phdr;
#else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Shdr Elf_Shdr;
typedef Elf32_Phdr Elf_Phdr;
#endif

static void *MCExecutableFindSection(const char *p_name)
{
	bool t_success;
	t_success = true;

	// It is not clear whether the section table on Linux is mapped into memory,
	// however this isn't really a problem, we just load it direct from the file.
	FILE *t_exe;
	t_exe = NULL;
	if (t_success)
	{
		t_exe = fopen(MCcmd, "rb");
		if (t_exe == NULL)
			t_success = false;
	}

	// Load the header
	Elf_Ehdr t_header;
	if (t_success)
		if (fread(&t_header, sizeof(Elf_Ehdr), 1, t_exe) != 1)
			t_success = false;

	// Allocate memory for the section table
	Elf_Shdr *t_sections;
	t_sections = nil;
	if (t_success)
		t_success = MCMemoryAllocate(sizeof(Elf_Shdr) * t_header . e_shnum, t_sections);

	// Now read in the sections
	for(uint32_t i = 0; i < t_header . e_shnum && t_success; i++)
	{
		if (fseek(t_exe, t_header . e_shoff + i * t_header . e_shentsize, SEEK_SET) != 0 ||
			fread(&t_sections[i], sizeof(Elf_Shdr), 1, t_exe) != 1)
			t_success = false;
	}

	// Next allocate memory for the string table, and read it in
	char *t_strings;
	t_strings = nil;
	if (t_success)
		t_success =
			MCMemoryAllocate(t_sections[t_header . e_shstrndx] . sh_size, t_strings) &&
			fseek(t_exe, t_sections[t_header . e_shstrndx] . sh_offset, SEEK_SET) == 0 &&
			fread(t_strings, t_sections[t_header . e_shstrndx] . sh_size, 1, t_exe) == 1;

	// Now we can search for our section
	void *t_address;
	t_address = NULL;
	for(uint32_t i = 0; i < t_header . e_shnum && t_success; i++)
		if (strcmp(p_name, t_strings + t_sections[i] . sh_name) == 0)
		{
			t_address = (void *)t_sections[i] . sh_addr;
			break;
		}

	// Free up all the resources we allocated
	MCMemoryDeallocate(t_strings);
	MCMemoryDeallocate(t_sections);
	if (t_exe != NULL)
		fclose(t_exe);

	return t_address;
}
#endif

