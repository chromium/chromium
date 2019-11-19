// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// =============================================================================
// PLEASE READ
//
// In general, you should not be adding stuff to this file.
//
// - If your thing is only used in one place, just put it in a reasonable
//   location in or near that one place. It's nice you want people to be able
//   to re-use your function, but realistically, if it hasn't been necessary
//   before after so many years of development, it's probably not going to be
//   used in other places in the future unless you know of them now.
//
// - If your thing is used by multiple callers and is UI-related, it should
//   probably be in app/win/ instead. Try to put it in the most specific file
//   possible (avoiding the *_util files when practical).
//
// =============================================================================

#ifndef BASE_WIN_WIN_UTIL_H_
#define BASE_WIN_WIN_UTIL_H_

#include <stdint.h>
#include "base/win/windows_types.h"

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"

struct IPropertyStore;
struct _tagpropertykey;
typedef _tagpropertykey PROPERTYKEY;

namespace base {

struct NativeLibraryLoadError;

namespace win {

inline uint32_t HandleToUint32(HANDLE h) {
  // Cast through uintptr_t and then unsigned int to make the truncation to
  // 32 bits explicit. Handles are size of-pointer but are always 32-bit values.
  // https://msdn.microsoft.com/en-us/library/aa384203(VS.85).aspx says:
  // 64-bit versions of Windows use 32-bit handles for interoperability.
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(h));
}

inline HANDLE Uint32ToHandle(uint32_t h) {
  return reinterpret_cast<HANDLE>(
      static_cast<uintptr_t>(static_cast<int32_t>(h)));
}

// Returns the string representing the current user sid. Does not modify
// |user_sid| on failure.
BASE_EXPORT bool GetUserSidString(std::wstring* user_sid);

// Returns false if user account control (UAC) has been disabled with the
// EnableLUA registry flag. Returns true if user account control is enabled.
// NOTE: The EnableLUA registry flag, which is ignored on Windows XP
// machines, might still exist and be set to 0 (UAC disabled), in which case
// this function will return false. You should therefore check this flag only
// if the OS is Vista or later.
BASE_EXPORT bool UserAccountControlIsEnabled();

// Sets the boolean value for a given key in given IPropertyStore.
BASE_EXPORT bool SetBooleanValueForPropertyStore(
    IPropertyStore* property_store,
    const PROPERTYKEY& property_key,
    bool property_bool_value);

// Sets the string value for a given key in given IPropertyStore.
BASE_EXPORT bool SetStringValueForPropertyStore(
    IPropertyStore* property_store,
    const PROPERTYKEY& property_key,
    const wchar_t* property_string_value);

// Sets the CLSID value for a given key in a given IPropertyStore.
BASE_EXPORT bool SetClsidForPropertyStore(IPropertyStore* property_store,
                                          const PROPERTYKEY& property_key,
                                          const CLSID& property_clsid_value);

// Sets the application id in given IPropertyStore. The function is intended
// for tagging application/chromium shortcut, browser window and jump list for
// Win7.
BASE_EXPORT bool SetAppIdForPropertyStore(IPropertyStore* property_store,
                                          const wchar_t* app_id);

// Adds the specified |command| using the specified |name| to the AutoRun key.
// |root_key| could be HKCU or HKLM or the root of any user hive.
BASE_EXPORT bool AddCommandToAutoRun(HKEY root_key,
                                     const std::wstring& name,
                                     const std::wstring& command);
// Removes the command specified by |name| from the AutoRun key. |root_key|
// could be HKCU or HKLM or the root of any user hive.
BASE_EXPORT bool RemoveCommandFromAutoRun(HKEY root_key,
                                          const std::wstring& name);

// Reads the command specified by |name| from the AutoRun key. |root_key|
// could be HKCU or HKLM or the root of any user hive. Used for unit-tests.
BASE_EXPORT bool ReadCommandFromAutoRun(HKEY root_key,
                                        const std::wstring& name,
                                        std::wstring* command);

// Sets whether to crash the process during exit. This is inspected by DLLMain
// and used to intercept unexpected terminations of the process (via calls to
// exit(), abort(), _exit(), ExitProcess()) and convert them into crashes.
// Note that not all mechanisms for terminating the process are covered by
// this. In particular, TerminateProcess() is not caught.
BASE_EXPORT void SetShouldCrashOnProcessDetach(bool crash);
BASE_EXPORT bool ShouldCrashOnProcessDetach();

// Adjusts the abort behavior so that crash reports can be generated when the
// process is aborted.
BASE_EXPORT void SetAbortBehaviorForCrashReporting();

// Checks whether the supplied |hwnd| is in Windows 10 tablet mode. Will return
// false on versions below 10.
BASE_EXPORT bool IsWindows10TabletMode(HWND hwnd);

// A tablet is a device that is touch enabled and also is being used
// "like a tablet". This is used by the following:
// 1. Metrics: To gain insight into how users use Chrome.
// 2. Physical keyboard presence: If a device is in tablet mode, it means
//    that there is no physical keyboard attached.
// This function optionally sets the |reason| parameter to determine as to why
// or why not a device was deemed to be a tablet.
// Returns true if the user has set Windows 10 in tablet mode.
BASE_EXPORT bool IsTabletDevice(std::string* reason, HWND hwnd);

// Return true if the device is physically used as a tablet independently of
// Windows tablet mode. It checks if the device:
// - Is running Windows 8 or newer,
// - Has a touch digitizer,
// - Is not docked,
// - Has a supported rotation sensor,
// - Is not in laptop mode,
// - prefers the mobile or slate power management profile (per OEM choice), and
// - Is in slate mode.
// This function optionally sets the |reason| parameter to determine as to why
// or why not a device was deemed to be a tablet.
BASE_EXPORT bool IsDeviceUsedAsATablet(std::string* reason);

// A slate is a touch device that may have a keyboard attached. This function
// returns true if a keyboard is attached and optionally will set the |reason|
// parameter to the detection method that was used to detect the keyboard.
BASE_EXPORT bool IsKeyboardPresentOnSlate(HWND hwnd, std::string* reason);

// Get the size of a struct up to and including the specified member.
// This is necessary to set compatible struct sizes for different versions
// of certain Windows APIs (e.g. SystemParametersInfo).
#define SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(struct_name, member) \
  offsetof(struct_name, member) +                                     \
      (sizeof static_cast<struct_name*>(NULL)->member)

// Returns true if the machine is enrolled to a domain.
BASE_EXPORT bool IsEnrolledToDomain();

// Returns true if the machine is being managed by an MDM system.
BASE_EXPORT bool IsDeviceRegisteredWithManagement();

// Returns true if the current process can make USER32 or GDI32 calls such as
// CreateWindow and CreateDC. Windows 8 and above allow the kernel component
// of these calls to be disabled (also known as win32k lockdown) which can
// cause undefined behaviour such as crashes. This function can be used to
// guard areas of code using these calls and provide a fallback path if
// necessary.
// Because they are not always needed (and not needed at all in processes that
// have the win32k lockdown), USER32 and GDI32 are delayloaded. Attempts to
// load them in those processes will cause a crash. Any code which uses USER32
// or GDI32 and may run in a locked-down process MUST be guarded using this
// method. Before the dlls were delayloaded, method calls into USER32 and GDI32
// did not work, so adding calls to this method to guard them simply avoids
// unnecessary method calls.
BASE_EXPORT bool IsUser32AndGdi32Available();

// Takes a snapshot of the modules loaded in the |process|. The returned
// HMODULEs are not add-ref'd, so they should not be closed and may be
// invalidated at any time (should a module be unloaded). |process| requires
// the PROCESS_QUERY_INFORMATION and PROCESS_VM_READ permissions.
BASE_EXPORT bool GetLoadedModulesSnapshot(HANDLE process,
                                          std::vector<HMODULE>* snapshot);

// Adds or removes the MICROSOFT_TABLETPENSERVICE_PROPERTY property with the
// TABLET_DISABLE_FLICKS & TABLET_DISABLE_FLICKFALLBACKKEYS flags in order to
// disable pen flick gestures for the given HWND.
BASE_EXPORT void EnableFlicks(HWND hwnd);
BASE_EXPORT void DisableFlicks(HWND hwnd);

// Returns true if the process is per monitor DPI aware.
BASE_EXPORT bool IsProcessPerMonitorDpiAware();

// Enable high-DPI support for the current process.
BASE_EXPORT void EnableHighDPISupport();

// Returns a string representation of |rguid|.
BASE_EXPORT string16 String16FromGUID(REFGUID rguid);

// Attempts to pin user32.dll to ensure it remains loaded. If it isn't loaded
// yet, the module will first be loaded and then the pin will be attempted. If
// pinning is successful, returns true. If the module cannot be loaded and/or
// pinned, |error| is set and the method returns false.
BASE_EXPORT bool PinUser32(NativeLibraryLoadError* error = nullptr);

// Gets a pointer to a function within user32.dll, if available. If user32.dll
// cannot be loaded or the function cannot be found, this function returns
// nullptr and sets |error|. Once loaded, user32.dll is pinned, and therefore
// the function pointer returned by this function will never change and can be
// cached.
BASE_EXPORT void* GetUser32FunctionPointer(
    const char* function_name,
    NativeLibraryLoadError* error = nullptr);

// Returns the name of a desktop or a window station.
BASE_EXPORT std::wstring GetWindowObjectName(HANDLE handle);

// Checks if the calling thread is running under a desktop with the name
// given by |desktop_name|. |desktop_name| is ASCII case insensitive (non-ASCII
// characters will be compared with exact matches).
BASE_EXPORT bool IsRunningUnderDesktopName(WStringPiece desktop_name);

// Returns true if current session is a remote session.
BASE_EXPORT bool IsCurrentSessionRemote();

// Allows changing the domain enrolled state for the life time of the object.
// The original state is restored upon destruction.
class BASE_EXPORT ScopedDomainStateForTesting {
 public:
  ScopedDomainStateForTesting(bool state);
  ~ScopedDomainStateForTesting();

 private:
  bool initial_state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDomainStateForTesting);
};

// Allows changing the management registration state for the life time of the
// object.  The original state is restored upon destruction.
class BASE_EXPORT ScopedDeviceRegisteredWithManagementForTesting {
 public:
  ScopedDeviceRegisteredWithManagementForTesting(bool state);
  ~ScopedDeviceRegisteredWithManagementForTesting();

 private:
  bool initial_state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDeviceRegisteredWithManagementForTesting);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_WIN_UTIL_H_
