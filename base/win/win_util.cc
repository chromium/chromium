// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/win_util.h"

#include <aclapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <powrprof.h>
#include <shobjidl.h>  // Must be before propkey.

#include <inspectable.h>
#include <mdmregistration.h>
#include <objbase.h>
#include <propkey.h>
#include <psapi.h>
#include <roapi.h>
#include <sddl.h>
#include <setupapi.h>
#include <shellscalingapi.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <strsafe.h>
#include <tchar.h>  // Must be before tpcshrd.h or for any use of _T macro
#include <tpcshrd.h>
#include <uiviewsettingsinterop.h>
#include <windows.ui.viewmanagement.h>
#include <winstring.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/propvarutil.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/shlwapi.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

namespace {

// Sets the value of |property_key| to |property_value| in |property_store|.
bool SetPropVariantValueForPropertyStore(
    IPropertyStore* property_store,
    const PROPERTYKEY& property_key,
    const ScopedPropVariant& property_value) {
  DCHECK(property_store);

  HRESULT result = property_store->SetValue(property_key, property_value.get());
  if (result == S_OK)
    result = property_store->Commit();
  if (SUCCEEDED(result))
    return true;
#if DCHECK_IS_ON()
  if (HRESULT_FACILITY(result) == FACILITY_WIN32)
    ::SetLastError(HRESULT_CODE(result));
  // See third_party/perl/c/i686-w64-mingw32/include/propkey.h for GUID and
  // PID definitions.
  DPLOG(ERROR) << "Failed to set property with GUID "
               << WStringFromGUID(property_key.fmtid) << " PID "
               << property_key.pid;
#endif
  return false;
}

void __cdecl ForceCrashOnSigAbort(int) {
  *((volatile int*)nullptr) = 0x1337;
}

// Returns the current platform role. We use the PowerDeterminePlatformRoleEx
// API for that.
POWER_PLATFORM_ROLE GetPlatformRole() {
  return PowerDeterminePlatformRoleEx(POWER_PLATFORM_ROLE_V2);
}

// Method used for Windows 8.1 and later.
// Since we support versions earlier than 8.1, we must dynamically load this
// function from user32.dll, so it won't fail to load in runtime. For earlier
// Windows versions GetProcAddress will return null and report failure so that
// callers can fall back on the deprecated SetProcessDPIAware.
bool SetProcessDpiAwarenessWrapper(PROCESS_DPI_AWARENESS value) {
  if (!IsUser32AndGdi32Available())
    return false;

  static const auto set_process_dpi_awareness_func =
      reinterpret_cast<decltype(&::SetProcessDpiAwareness)>(
          GetUser32FunctionPointer("SetProcessDpiAwarenessInternal"));
  if (set_process_dpi_awareness_func) {
    HRESULT hr = set_process_dpi_awareness_func(value);
    if (SUCCEEDED(hr))
      return true;
    DLOG_IF(ERROR, hr == E_ACCESSDENIED)
        << "Access denied error from SetProcessDpiAwarenessInternal. "
           "Function called twice, or manifest was used.";
    NOTREACHED()
        << "SetProcessDpiAwarenessInternal failed with unexpected error: "
        << hr;
    return false;
  }

  DCHECK_LT(GetVersion(), Version::WIN8_1) << "SetProcessDpiAwarenessInternal "
                                              "should be available on all "
                                              "platforms >= Windows 8.1";
  return false;
}

// Enable V2 per-monitor high-DPI support for the process. This will cause
// Windows to scale dialogs, comctl32 controls, context menus, and non-client
// area owned by this process on a per-monitor basis. If per-monitor V2 is not
// available (i.e., prior to Windows 10 1703) or fails, returns false.
// https://docs.microsoft.com/en-us/windows/desktop/hidpi/dpi-awareness-context
bool EnablePerMonitorV2() {
  if (!IsUser32AndGdi32Available())
    return false;

  static const auto set_process_dpi_awareness_context_func =
      reinterpret_cast<decltype(&::SetProcessDpiAwarenessContext)>(
          GetUser32FunctionPointer("SetProcessDpiAwarenessContext"));
  if (set_process_dpi_awareness_context_func) {
    return set_process_dpi_awareness_context_func(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  }

  DCHECK_LT(GetVersion(), Version::WIN10_RS2)
      << "SetProcessDpiAwarenessContext should be available on all platforms"
         " >= Windows 10 Redstone 2";

  return false;
}

bool* GetDomainEnrollmentStateStorage() {
  static bool state = IsOS(OS_DOMAINMEMBER);
  return &state;
}

bool* GetRegisteredWithManagementStateStorage() {
  static bool state = []() {
    // Mitigate the issues caused by loading DLLs on a background thread
    // (http://crbug/973868).
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    ScopedNativeLibrary library(
        FilePath(FILE_PATH_LITERAL("MDMRegistration.dll")));
    if (!library.is_valid())
      return false;

    using IsDeviceRegisteredWithManagementFunction =
        decltype(&::IsDeviceRegisteredWithManagement);
    IsDeviceRegisteredWithManagementFunction
        is_device_registered_with_management_function =
            reinterpret_cast<IsDeviceRegisteredWithManagementFunction>(
                library.GetFunctionPointer("IsDeviceRegisteredWithManagement"));
    if (!is_device_registered_with_management_function)
      return false;

    BOOL is_managed = FALSE;
    HRESULT hr =
        is_device_registered_with_management_function(&is_managed, 0, nullptr);
    return SUCCEEDED(hr) && is_managed;
  }();

  return &state;
}

NativeLibrary PinUser32Internal(NativeLibraryLoadError* error) {
  static NativeLibraryLoadError load_error;
  static const NativeLibrary user32_module =
      PinSystemLibrary(FILE_PATH_LITERAL("user32.dll"), &load_error);
  if (!user32_module && error)
    error->code = load_error.code;
  return user32_module;
}

}  // namespace

// Uses the Windows 10 WRL API's to query the current system state. The API's
// we are using in the function below are supported in Win32 apps as per msdn.
// It looks like the API implementation is buggy at least on Surface 4 causing
// it to always return UserInteractionMode_Touch which as per documentation
// indicates tablet mode.
bool IsWindows10TabletMode(HWND hwnd) {
  if (GetVersion() < Version::WIN10)
    return false;

  if (!ResolveCoreWinRTDelayload() ||
      !ScopedHString::ResolveCoreWinRTStringDelayload()) {
    return false;
  }

  ScopedHString view_settings_guid = ScopedHString::Create(
      RuntimeClass_Windows_UI_ViewManagement_UIViewSettings);
  Microsoft::WRL::ComPtr<IUIViewSettingsInterop> view_settings_interop;
  HRESULT hr = win::RoGetActivationFactory(
      view_settings_guid.get(), IID_PPV_ARGS(&view_settings_interop));
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IUIViewSettings>
      view_settings;
  hr = view_settings_interop->GetForWindow(hwnd, IID_PPV_ARGS(&view_settings));
  if (FAILED(hr))
    return false;

  ABI::Windows::UI::ViewManagement::UserInteractionMode mode =
      ABI::Windows::UI::ViewManagement::UserInteractionMode_Mouse;
  view_settings->get_UserInteractionMode(&mode);
  return mode == ABI::Windows::UI::ViewManagement::UserInteractionMode_Touch;
}

// Returns true if a physical keyboard is detected on Windows 8 and up.
// Uses the Setup APIs to enumerate the attached keyboards and returns true
// if the keyboard count is 1 or more.. While this will work in most cases
// it won't work if there are devices which expose keyboard interfaces which
// are attached to the machine.
bool IsKeyboardPresentOnSlate(HWND hwnd, std::string* reason) {
  bool result = false;

  if (GetVersion() < Version::WIN8) {
    if (reason)
      *reason = "Detection not supported";
    return false;
  }

  // This function is only supported for Windows 8 and up.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableUsbKeyboardDetect)) {
    if (reason)
      *reason = "Detection disabled";
    return false;
  }

  // This function should be only invoked for machines with touch screens.
  if ((GetSystemMetrics(SM_DIGITIZER) & NID_INTEGRATED_TOUCH) !=
      NID_INTEGRATED_TOUCH) {
    if (reason) {
      *reason += "NID_INTEGRATED_TOUCH\n";
      result = true;
    } else {
      return true;
    }
  }

  // If it is a tablet device we assume that there is no keyboard attached.
  if (IsTabletDevice(reason, hwnd)) {
    if (reason)
      *reason += "Tablet device.\n";
    return false;
  }

  if (!reason)
    return true;

  *reason += "Not a tablet device";
  result = true;

  // To determine whether a keyboard is present on the device, we do the
  // following:-
  // 1. Check whether the device supports auto rotation. If it does then
  //    it possibly supports flipping from laptop to slate mode. If it
  //    does not support auto rotation, then we assume it is a desktop
  //    or a normal laptop and assume that there is a keyboard.

  // 2. If the device supports auto rotation, then we get its platform role
  //    and check the system metric SM_CONVERTIBLESLATEMODE to see if it is
  //    being used in slate mode. If yes then we return false here to ensure
  //    that the OSK is displayed.

  // 3. If step 1 and 2 fail then we check attached keyboards and return true
  //    if we find ACPI\* or HID\VID* keyboards.

  using GetAutoRotationState = decltype(&::GetAutoRotationState);
  static const auto get_rotation_state = reinterpret_cast<GetAutoRotationState>(
      GetUser32FunctionPointer("GetAutoRotationState"));
  if (get_rotation_state) {
    AR_STATE auto_rotation_state = AR_ENABLED;
    get_rotation_state(&auto_rotation_state);
    if ((auto_rotation_state & AR_NOSENSOR) ||
        (auto_rotation_state & AR_NOT_SUPPORTED)) {
      // If there is no auto rotation sensor or rotation is not supported in
      // the current configuration, then we can assume that this is a desktop
      // or a traditional laptop.
      if (!reason)
        return true;

      *reason += (auto_rotation_state & AR_NOSENSOR) ? "AR_NOSENSOR\n"
                                                     : "AR_NOT_SUPPORTED\n";
      result = true;
    }
  }

  const GUID KEYBOARD_CLASS_GUID = {
      0x4D36E96B,
      0xE325,
      0x11CE,
      {0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18}};

  // Query for all the keyboard devices.
  HDEVINFO device_info = SetupDiGetClassDevs(&KEYBOARD_CLASS_GUID, nullptr,
                                             nullptr, DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE) {
    if (reason)
      *reason += "No keyboard info\n";
    return result;
  }

  // Enumerate all keyboards and look for ACPI\PNP and HID\VID devices. If
  // the count is more than 1 we assume that a keyboard is present. This is
  // under the assumption that there will always be one keyboard device.
  for (DWORD i = 0;; ++i) {
    SP_DEVINFO_DATA device_info_data = {0};
    device_info_data.cbSize = sizeof(device_info_data);
    if (!SetupDiEnumDeviceInfo(device_info, i, &device_info_data))
      break;

    // Get the device ID.
    wchar_t device_id[MAX_DEVICE_ID_LEN];
    CONFIGRET status = CM_Get_Device_ID(device_info_data.DevInst, device_id,
                                        MAX_DEVICE_ID_LEN, 0);
    if (status == CR_SUCCESS) {
      // To reduce the scope of the hack we only look for ACPI and HID\\VID
      // prefixes in the keyboard device ids.
      if (StartsWith(device_id, L"ACPI", CompareCase::INSENSITIVE_ASCII) ||
          StartsWith(device_id, L"HID\\VID", CompareCase::INSENSITIVE_ASCII)) {
        if (reason) {
          *reason += "device: ";
          *reason += WideToUTF8(device_id);
          *reason += '\n';
        }
        // The heuristic we are using is to check the count of keyboards and
        // return true if the API's report one or more keyboards. Please note
        // that this will break for non keyboard devices which expose a
        // keyboard PDO.
        result = true;
      }
    }
  }
  return result;
}

static bool g_crash_on_process_detach = false;

bool GetUserSidString(std::wstring* user_sid) {
  // Get the current token.
  HANDLE token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  ScopedHandle token_scoped(token);

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  std::unique_ptr<BYTE[]> user_bytes(new BYTE[size]);
  TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes.get());

  if (!::GetTokenInformation(token, TokenUser, user, size, &size))
    return false;

  if (!user->User.Sid)
    return false;

  // Convert the data to a string.
  wchar_t* sid_string;
  if (!::ConvertSidToStringSid(user->User.Sid, &sid_string))
    return false;

  *user_sid = sid_string;

  ::LocalFree(sid_string);

  return true;
}

bool UserAccountControlIsEnabled() {
  // This can be slow if Windows ends up going to disk.  Should watch this key
  // for changes and only read it once, preferably on the file thread.
  //   http://code.google.com/p/chromium/issues/detail?id=61644
  ThreadRestrictions::ScopedAllowIO allow_io;

  RegKey key(HKEY_LOCAL_MACHINE,
             L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
             KEY_READ);
  DWORD uac_enabled;
  if (key.ReadValueDW(L"EnableLUA", &uac_enabled) != ERROR_SUCCESS) {
    return true;
  }
  // Users can set the EnableLUA value to something arbitrary, like 2, which
  // Vista will treat as UAC enabled, so we make sure it is not set to 0.
  return (uac_enabled != 0);
}

bool SetBooleanValueForPropertyStore(IPropertyStore* property_store,
                                     const PROPERTYKEY& property_key,
                                     bool property_bool_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromBoolean(property_bool_value,
                                        property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store, property_key,
                                             property_value);
}

bool SetStringValueForPropertyStore(IPropertyStore* property_store,
                                    const PROPERTYKEY& property_key,
                                    const wchar_t* property_string_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromString(property_string_value,
                                       property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store, property_key,
                                             property_value);
}

bool SetClsidForPropertyStore(IPropertyStore* property_store,
                              const PROPERTYKEY& property_key,
                              const CLSID& property_clsid_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromCLSID(property_clsid_value,
                                      property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store, property_key,
                                             property_value);
}

bool SetAppIdForPropertyStore(IPropertyStore* property_store,
                              const wchar_t* app_id) {
  // App id should be less than 128 chars and contain no space. And recommended
  // format is CompanyName.ProductName[.SubProduct.ProductNumber].
  // See
  // https://docs.microsoft.com/en-us/windows/win32/shell/appids#how-to-form-an-application-defined-appusermodelid
  DCHECK_LT(lstrlen(app_id), 128);
  DCHECK_EQ(wcschr(app_id, L' '), nullptr);

  return SetStringValueForPropertyStore(property_store, PKEY_AppUserModel_ID,
                                        app_id);
}

static const wchar_t kAutoRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool AddCommandToAutoRun(HKEY root_key,
                         const std::wstring& name,
                         const std::wstring& command) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return (autorun_key.WriteValue(name.c_str(), command.c_str()) ==
          ERROR_SUCCESS);
}

bool RemoveCommandFromAutoRun(HKEY root_key, const std::wstring& name) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return (autorun_key.DeleteValue(name.c_str()) == ERROR_SUCCESS);
}

bool ReadCommandFromAutoRun(HKEY root_key,
                            const std::wstring& name,
                            std::wstring* command) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_QUERY_VALUE);
  return (autorun_key.ReadValue(name.c_str(), command) == ERROR_SUCCESS);
}

void SetShouldCrashOnProcessDetach(bool crash) {
  g_crash_on_process_detach = crash;
}

bool ShouldCrashOnProcessDetach() {
  return g_crash_on_process_detach;
}

void SetAbortBehaviorForCrashReporting() {
  // Prevent CRT's abort code from prompting a dialog or trying to "report" it.
  // Disabling the _CALL_REPORTFAULT behavior is important since otherwise it
  // has the sideffect of clearing our exception filter, which means we
  // don't get any crash.
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

  // Set a SIGABRT handler for good measure. We will crash even if the default
  // is left in place, however this allows us to crash earlier. And it also
  // lets us crash in response to code which might directly call raise(SIGABRT)
  signal(SIGABRT, ForceCrashOnSigAbort);
}

bool IsTabletDevice(std::string* reason, HWND hwnd) {
  if (GetVersion() < Version::WIN8) {
    if (reason)
      *reason = "Tablet device detection not supported below Windows 8\n";
    return false;
  }

  if (IsWindows10TabletMode(hwnd))
    return true;

  return IsDeviceUsedAsATablet(reason);
}

// This method is used to set the right interactions media queries,
// see https://drafts.csswg.org/mediaqueries-4/#mf-interaction. It doesn't
// check the Windows 10 tablet mode because it doesn't reflect the actual
// input configuration of the device and can be manually triggered by the user
// independently from the hardware state.
bool IsDeviceUsedAsATablet(std::string* reason) {
  if (GetVersion() < Version::WIN8) {
    if (reason)
      *reason = "Tablet device detection not supported below Windows 8\n";
    return false;
  }

  if (GetSystemMetrics(SM_MAXIMUMTOUCHES) == 0) {
    if (reason) {
      *reason += "Device does not support touch.\n";
    } else {
      return false;
    }
  }

  // If the device is docked, the user is treating the device as a PC.
  if (GetSystemMetrics(SM_SYSTEMDOCKED) != 0) {
    if (reason) {
      *reason += "SM_SYSTEMDOCKED\n";
    } else {
      return false;
    }
  }

  // If the device is not supporting rotation, it's unlikely to be a tablet,
  // a convertible or a detachable.
  // See
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dn629263(v=vs.85).aspx
  using GetAutoRotationStateType = decltype(GetAutoRotationState)*;
  static const auto get_auto_rotation_state_func =
      reinterpret_cast<GetAutoRotationStateType>(
          GetUser32FunctionPointer("GetAutoRotationState"));
  if (get_auto_rotation_state_func) {
    AR_STATE rotation_state = AR_ENABLED;
    if (get_auto_rotation_state_func(&rotation_state) &&
        (rotation_state & (AR_NOT_SUPPORTED | AR_LAPTOP | AR_NOSENSOR)) != 0)
      return false;
  }

  // PlatformRoleSlate was added in Windows 8+.
  POWER_PLATFORM_ROLE role = GetPlatformRole();
  bool is_tablet = false;
  if (role == PlatformRoleMobile || role == PlatformRoleSlate) {
    is_tablet = !GetSystemMetrics(SM_CONVERTIBLESLATEMODE);
    if (!is_tablet) {
      if (reason) {
        *reason += "Not in slate mode.\n";
      } else {
        return false;
      }
    } else {
      if (reason) {
        *reason += (role == PlatformRoleMobile) ? "PlatformRoleMobile\n"
                                                : "PlatformRoleSlate\n";
      }
    }
  } else {
    if (reason)
      *reason += "Device role is not mobile or slate.\n";
  }
  return is_tablet;
}

bool IsEnrolledToDomain() {
  return *GetDomainEnrollmentStateStorage();
}

bool IsDeviceRegisteredWithManagement() {
  return *GetRegisteredWithManagementStateStorage();
}

bool IsUser32AndGdi32Available() {
  static auto is_user32_and_gdi32_available = []() {
    // If win32k syscalls aren't disabled, then user32 and gdi32 are available.

    // Can't disable win32k prior to windows 8.
    if (GetVersion() < Version::WIN8)
      return true;

    using GetProcessMitigationPolicyType =
        decltype(GetProcessMitigationPolicy)*;
    GetProcessMitigationPolicyType get_process_mitigation_policy_func =
        reinterpret_cast<GetProcessMitigationPolicyType>(GetProcAddress(
            GetModuleHandle(L"kernel32.dll"), "GetProcessMitigationPolicy"));

    if (!get_process_mitigation_policy_func)
      return true;

    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
    if (get_process_mitigation_policy_func(GetCurrentProcess(),
                                           ProcessSystemCallDisablePolicy,
                                           &policy, sizeof(policy))) {
      return policy.DisallowWin32kSystemCalls == 0;
    }

    return true;
  }();
  return is_user32_and_gdi32_available;
}

bool GetLoadedModulesSnapshot(HANDLE process, std::vector<HMODULE>* snapshot) {
  DCHECK(snapshot);
  DCHECK_EQ(0u, snapshot->size());
  snapshot->resize(128);

  // We will retry at least once after first determining |bytes_required|. If
  // the list of modules changes after we receive |bytes_required| we may retry
  // more than once.
  int retries_remaining = 5;
  do {
    DWORD bytes_required = 0;
    // EnumProcessModules returns 'success' even if the buffer size is too
    // small.
    DCHECK_GE(std::numeric_limits<DWORD>::max(),
              snapshot->size() * sizeof(HMODULE));
    if (!::EnumProcessModules(
            process, &(*snapshot)[0],
            static_cast<DWORD>(snapshot->size() * sizeof(HMODULE)),
            &bytes_required)) {
      DPLOG(ERROR) << "::EnumProcessModules failed.";
      return false;
    }
    DCHECK_EQ(0u, bytes_required % sizeof(HMODULE));
    size_t num_modules = bytes_required / sizeof(HMODULE);
    if (num_modules <= snapshot->size()) {
      // Buffer size was too big, presumably because a module was unloaded.
      snapshot->erase(snapshot->begin() + num_modules, snapshot->end());
      return true;
    } else if (num_modules == 0) {
      DLOG(ERROR) << "Can't determine the module list size.";
      return false;
    } else {
      // Buffer size was too small. Try again with a larger buffer. A little
      // more room is given to avoid multiple expensive calls to
      // ::EnumProcessModules() just because one module has been added.
      snapshot->resize(num_modules + 8, nullptr);
    }
  } while (--retries_remaining);

  DLOG(ERROR) << "Failed to enumerate modules.";
  return false;
}

void EnableFlicks(HWND hwnd) {
  ::RemoveProp(hwnd, MICROSOFT_TABLETPENSERVICE_PROPERTY);
}

void DisableFlicks(HWND hwnd) {
  ::SetProp(hwnd, MICROSOFT_TABLETPENSERVICE_PROPERTY,
            reinterpret_cast<HANDLE>(TABLET_DISABLE_FLICKS |
                                     TABLET_DISABLE_FLICKFALLBACKKEYS));
}

bool IsProcessPerMonitorDpiAware() {
  enum class PerMonitorDpiAware {
    UNKNOWN = 0,
    PER_MONITOR_DPI_UNAWARE,
    PER_MONITOR_DPI_AWARE,
  };
  static PerMonitorDpiAware per_monitor_dpi_aware = PerMonitorDpiAware::UNKNOWN;
  if (per_monitor_dpi_aware == PerMonitorDpiAware::UNKNOWN) {
    per_monitor_dpi_aware = PerMonitorDpiAware::PER_MONITOR_DPI_UNAWARE;
    HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
    if (shcore_dll) {
      auto get_process_dpi_awareness_func =
          reinterpret_cast<decltype(::GetProcessDpiAwareness)*>(
              ::GetProcAddress(shcore_dll, "GetProcessDpiAwareness"));
      if (get_process_dpi_awareness_func) {
        PROCESS_DPI_AWARENESS awareness;
        if (SUCCEEDED(get_process_dpi_awareness_func(nullptr, &awareness)) &&
            awareness == PROCESS_PER_MONITOR_DPI_AWARE)
          per_monitor_dpi_aware = PerMonitorDpiAware::PER_MONITOR_DPI_AWARE;
      }
    }
  }
  return per_monitor_dpi_aware == PerMonitorDpiAware::PER_MONITOR_DPI_AWARE;
}

void EnableHighDPISupport() {
  if (!IsUser32AndGdi32Available())
    return;

  // Enable per-monitor V2 if it is available (Win10 1703 or later).
  if (EnablePerMonitorV2())
    return;

  // Fall back to per-monitor DPI for older versions of Win10 instead of
  // Win8.1 since Win8.1 does not have EnableChildWindowDpiMessage,
  // necessary for correct non-client area scaling across monitors.
  PROCESS_DPI_AWARENESS process_dpi_awareness =
      GetVersion() >= Version::WIN10 ? PROCESS_PER_MONITOR_DPI_AWARE
                                     : PROCESS_SYSTEM_DPI_AWARE;
  if (!SetProcessDpiAwarenessWrapper(process_dpi_awareness)) {
    // For windows versions where SetProcessDpiAwareness is not available or
    // failed, try its predecessor.
    BOOL result = ::SetProcessDPIAware();
    DCHECK(result) << "SetProcessDPIAware failed.";
  }
}

std::wstring WStringFromGUID(REFGUID rguid) {
  // This constant counts the number of characters in the formatted string,
  // including the null termination character.
  constexpr int kGuidStringCharacters =
      1 + 8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12 + 1 + 1;
  wchar_t guid_string[kGuidStringCharacters];
  CHECK(SUCCEEDED(StringCchPrintfW(
      guid_string, kGuidStringCharacters,
      L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", rguid.Data1,
      rguid.Data2, rguid.Data3, rguid.Data4[0], rguid.Data4[1], rguid.Data4[2],
      rguid.Data4[3], rguid.Data4[4], rguid.Data4[5], rguid.Data4[6],
      rguid.Data4[7])));
  return std::wstring(guid_string, kGuidStringCharacters - 1);
}

bool PinUser32(NativeLibraryLoadError* error) {
  return PinUser32Internal(error) != nullptr;
}

void* GetUser32FunctionPointer(const char* function_name,
                               NativeLibraryLoadError* error) {
  NativeLibrary user32_module = PinUser32Internal(error);
  if (user32_module)
    return GetFunctionPointerFromNativeLibrary(user32_module, function_name);
  return nullptr;
}

std::wstring GetWindowObjectName(HANDLE handle) {
  // Get the size of the name.
  std::wstring object_name;

  DWORD size = 0;
  ::GetUserObjectInformation(handle, UOI_NAME, nullptr, 0, &size);
  if (!size) {
    DPCHECK(false);
    return object_name;
  }

  LOG_ASSERT(size % sizeof(wchar_t) == 0u);

  // Query the name of the object.
  if (!::GetUserObjectInformation(
          handle, UOI_NAME, WriteInto(&object_name, size / sizeof(wchar_t)),
          size, &size)) {
    DPCHECK(false);
  }

  return object_name;
}

bool IsRunningUnderDesktopName(WStringPiece desktop_name) {
  HDESK thread_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
  if (!thread_desktop)
    return false;

  std::wstring current_desktop_name = GetWindowObjectName(thread_desktop);
  return EqualsCaseInsensitiveASCII(AsStringPiece16(current_desktop_name),
                                    AsStringPiece16(desktop_name));
}

// This method is used to detect whether current session is a remote session.
// See:
// https://docs.microsoft.com/en-us/windows/desktop/TermServ/detecting-the-terminal-services-environment
bool IsCurrentSessionRemote() {
  if (::GetSystemMetrics(SM_REMOTESESSION))
    return true;

  DWORD current_session_id = 0;

  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &current_session_id))
    return false;

  static constexpr wchar_t kRdpSettingsKeyName[] =
      L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server";
  RegKey key(HKEY_LOCAL_MACHINE, kRdpSettingsKeyName, KEY_READ);
  if (!key.Valid())
    return false;

  static constexpr wchar_t kGlassSessionIdValueName[] = L"GlassSessionId";
  DWORD glass_session_id = 0;
  if (key.ReadValueDW(kGlassSessionIdValueName, &glass_session_id) !=
      ERROR_SUCCESS)
    return false;

  return current_session_id != glass_session_id;
}

ScopedDomainStateForTesting::ScopedDomainStateForTesting(bool state)
    : initial_state_(IsEnrolledToDomain()) {
  *GetDomainEnrollmentStateStorage() = state;
}

ScopedDomainStateForTesting::~ScopedDomainStateForTesting() {
  *GetDomainEnrollmentStateStorage() = initial_state_;
}

ScopedDeviceRegisteredWithManagementForTesting::
    ScopedDeviceRegisteredWithManagementForTesting(bool state)
    : initial_state_(IsDeviceRegisteredWithManagement()) {
  *GetRegisteredWithManagementStateStorage() = state;
}

ScopedDeviceRegisteredWithManagementForTesting::
    ~ScopedDeviceRegisteredWithManagementForTesting() {
  *GetRegisteredWithManagementStateStorage() = initial_state_;
}

}  // namespace win
}  // namespace base
