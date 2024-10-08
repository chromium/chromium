// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/win/win_util.h"

#include <objbase.h>

#include <initguid.h>
#include <shobjidl.h>
#include <tchar.h>

#include <aclapi.h>
#include <cfgmgr32.h>
#include <inspectable.h>
#include <lm.h>
#include <mdmregistration.h>
#include <powrprof.h>
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
#include <tpcshrd.h>
#include <uiviewsettingsinterop.h>
#include <windows.ui.viewmanagement.h>
#include <winstring.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "base/win/access_token.h"
#include "base/win/core_winrt_util.h"
#include "base/win/propvarutil.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/shlwapi.h"
#include "base/win/static_constants.h"
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
  static bool state = [] {
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

// TODO (crbug/1300219): return a DSREG_JOIN_TYPE* instead of bool*.
bool* GetAzureADJoinStateStorage() {
  static bool state = [] {
    base::ElapsedTimer timer;

    // Mitigate the issues caused by loading DLLs on a background thread
    // (http://crbug/973868).
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    ScopedNativeLibrary netapi32(
        base::LoadSystemLibrary(FILE_PATH_LITERAL("netapi32.dll")));
    if (!netapi32.is_valid())
      return false;

    const auto net_get_aad_join_information_function =
        reinterpret_cast<decltype(&::NetGetAadJoinInformation)>(
            netapi32.GetFunctionPointer("NetGetAadJoinInformation"));
    if (!net_get_aad_join_information_function)
      return false;

    const auto net_free_aad_join_information_function =
        reinterpret_cast<decltype(&::NetFreeAadJoinInformation)>(
            netapi32.GetFunctionPointer("NetFreeAadJoinInformation"));
    DPCHECK(net_free_aad_join_information_function);

    DSREG_JOIN_INFO* join_info = nullptr;
    HRESULT hr = net_get_aad_join_information_function(/*pcszTenantId=*/nullptr,
                                                       &join_info);
    const bool is_aad_joined = SUCCEEDED(hr) && join_info;
    if (join_info) {
      net_free_aad_join_information_function(join_info);
    }

    base::UmaHistogramTimes("EnterpriseCheck.AzureADJoinStatusCheckTime",
                            timer.Elapsed());
    return is_aad_joined;
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
bool IsWindows10OrGreaterTabletMode(HWND hwnd) {
  if (GetVersion() >= Version::WIN11) {
    // Only Win10 supports explicit tablet mode. On Win11,
    // get_UserInteractionMode always returns UserInteractionMode_Mouse, so
    // instead we check if we're in slate mode or not - 0 value means slate
    // mode. See
    // https://docs.microsoft.com/en-us/windows-hardware/customize/desktop/unattend/microsoft-windows-gpiobuttons-convertibleslatemode

    constexpr int kKeyboardPresent = 1;
    base::win::RegKey registry_key(
        HKEY_LOCAL_MACHINE,
        L"System\\CurrentControlSet\\Control\\PriorityControl", KEY_READ);
    DWORD slate_mode = 0;
    bool value_exists = registry_key.ReadValueDW(L"ConvertibleSlateMode",
                                                 &slate_mode) == ERROR_SUCCESS;
    // Some devices don't set the reg key to 1 for keyboard-only devices, so
    // also check if the device is used as a tablet if it is not 1. Some devices
    // don't set the registry key at all; fall back to checking if the device
    // is used as a tablet for them as well.
    return !(value_exists && slate_mode == kKeyboardPresent) &&
           IsDeviceUsedAsATablet(/*reason=*/nullptr);
  }

  ScopedHString view_settings_guid = ScopedHString::Create(
      RuntimeClass_Windows_UI_ViewManagement_UIViewSettings);
  Microsoft::WRL::ComPtr<IUIViewSettingsInterop> view_settings_interop;
  HRESULT hr = ::RoGetActivationFactory(view_settings_guid.get(),
                                        IID_PPV_ARGS(&view_settings_interop));
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

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableUsbKeyboardDetect)) {
    if (reason) {
      *reason = "Detection disabled";
    }
    return false;
  }

  // This function should be only invoked for machines with touch screens.
  if ((GetSystemMetrics(SM_DIGITIZER) & NID_INTEGRATED_TOUCH) !=
      NID_INTEGRATED_TOUCH) {
    if (!reason) {
      return true;
    }

    *reason += "NID_INTEGRATED_TOUCH\n";
    result = true;
  }

  // If it is a tablet device we assume that there is no keyboard attached.
  if (IsTabletDevice(reason, hwnd)) {
    if (reason) {
      *reason += "Tablet device.\n";
    }
    return false;
  }

  if (!reason) {
    return true;
  }

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
      if (!reason) {
        return true;
      }

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
    if (reason) {
      *reason += "No keyboard info\n";
    }
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
  std::optional<AccessToken> token = AccessToken::FromCurrentProcess();
  if (!token)
    return false;
  std::optional<std::wstring> sid_string = token->User().ToSddlString();
  if (!sid_string)
    return false;
  *user_sid = *sid_string;
  return true;
}

class ScopedAllowBlockingForUserAccountControl : public ScopedAllowBlocking {};

bool UserAccountControlIsEnabled() {
  // This can be slow if Windows ends up going to disk.  Should watch this key
  // for changes and only read it once, preferably on the file thread.
  //   http://code.google.com/p/chromium/issues/detail?id=61644
  ScopedAllowBlockingForUserAccountControl allow_blocking;

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
  if (IsWindows10OrGreaterTabletMode(hwnd))
    return true;

  return IsDeviceUsedAsATablet(reason);
}

// This method is used to set the right interactions media queries,
// see https://drafts.csswg.org/mediaqueries-4/#mf-interaction. It doesn't
// check the Windows 10 tablet mode because it doesn't reflect the actual
// input configuration of the device and can be manually triggered by the user
// independently from the hardware state.
bool IsDeviceUsedAsATablet(std::string* reason) {
  // Once this is set, it shouldn't be overridden, and it should be the ultimate
  // return value, so that this method returns the same result whether or not
  // reason is NULL.
  std::optional<bool> ret;

  if (GetSystemMetrics(SM_MAXIMUMTOUCHES) == 0) {
    if (!reason) {
      return false;
    }

    *reason += "Device does not support touch.\n";
    ret = false;
  }

  // If the device is docked, the user is treating the device as a PC.
  if (GetSystemMetrics(SM_SYSTEMDOCKED) != 0) {
    if (!reason) {
      return false;
    }

    *reason += "SM_SYSTEMDOCKED\n";
    if (!ret.has_value()) {
      ret = false;
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
        (rotation_state & (AR_NOT_SUPPORTED | AR_LAPTOP | AR_NOSENSOR)) != 0) {
      return ret.value_or(false);
    }
  }

  // PlatformRoleSlate was added in Windows 8+.
  POWER_PLATFORM_ROLE role = GetPlatformRole();
  bool is_tablet = false;
  if (role == PlatformRoleMobile || role == PlatformRoleSlate) {
    is_tablet = !GetSystemMetrics(SM_CONVERTIBLESLATEMODE);
    if (!is_tablet) {
      if (!reason) {
        return false;
      }

      *reason += "Not in slate mode.\n";
      if (!ret.has_value()) {
        ret = false;
      }
    } else if (reason) {
      *reason += (role == PlatformRoleMobile) ? "PlatformRoleMobile\n"
                                              : "PlatformRoleSlate\n";
    }
  } else if (reason) {
    *reason += "Device role is not mobile or slate.\n";
  }
  return ret.value_or(is_tablet);
}

bool IsEnrolledToDomain() {
  return *GetDomainEnrollmentStateStorage();
}

bool IsDeviceRegisteredWithManagement() {
  // GetRegisteredWithManagementStateStorage() can be true for devices running
  // the Home sku, however the Home sku does not allow for management of the web
  // browser. As such, we automatically exclude devices running the Home sku.
  if (OSInfo::GetInstance()->version_type() == SUITE_HOME)
    return false;
  return *GetRegisteredWithManagementStateStorage();
}

bool IsJoinedToAzureAD() {
  return *GetAzureADJoinStateStorage();
}

bool IsUser32AndGdi32Available() {
  static const bool is_user32_and_gdi32_available = [] {
    // If win32k syscalls aren't disabled, then user32 and gdi32 are available.
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
    if (::GetProcessMitigationPolicy(GetCurrentProcess(),
                                     ProcessSystemCallDisablePolicy, &policy,
                                     sizeof(policy))) {
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
      snapshot->erase(snapshot->begin() + static_cast<ptrdiff_t>(num_modules),
                      snapshot->end());
      return true;
    }

    if (num_modules == 0) {
      DLOG(ERROR) << "Can't determine the module list size.";
      return false;
    }

    // Buffer size was too small. Try again with a larger buffer. A little
    // more room is given to avoid multiple expensive calls to
    // ::EnumProcessModules() just because one module has been added.
    snapshot->resize(num_modules + 8, nullptr);
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

void EnableHighDPISupport() {
  if (!IsUser32AndGdi32Available())
    return;

  // Enable per-monitor V2 if it is available (Win10 1703 or later).
  if (EnablePerMonitorV2())
    return;

  // Fall back to per-monitor DPI for older versions of Win10.
  PROCESS_DPI_AWARENESS process_dpi_awareness = PROCESS_PER_MONITOR_DPI_AWARE;
  if (!::SetProcessDpiAwareness(process_dpi_awareness)) {
    // For windows versions where SetProcessDpiAwareness fails, try its
    // predecessor.
    BOOL result = ::SetProcessDPIAware();
    DCHECK(result) << "SetProcessDPIAware failed.";
  }
}

std::wstring WStringFromGUID(const ::GUID& rguid) {
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

bool GetPointerDevice(HANDLE device, POINTER_DEVICE_INFO& result) {
  return ::GetPointerDevice(device, &result);
}

std::optional<std::vector<POINTER_DEVICE_INFO>> GetPointerDevices() {
  uint32_t device_count;
  if (!::GetPointerDevices(&device_count, nullptr)) {
    return std::nullopt;
  }

  std::vector<POINTER_DEVICE_INFO> pointer_devices(device_count);
  if (!::GetPointerDevices(&device_count, pointer_devices.data())) {
    return std::nullopt;
  }
  return pointer_devices;
}

bool RegisterPointerDeviceNotifications(HWND hwnd,
                                        bool notify_proximity_changes) {
  return ::RegisterPointerDeviceNotifications(hwnd, notify_proximity_changes);
}

bool IsRunningUnderDesktopName(std::wstring_view desktop_name) {
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
      ERROR_SUCCESS) {
    return false;
  }

  return current_session_id != glass_session_id;
}

bool IsAppVerifierLoaded() {
  return GetModuleHandleA(kApplicationVerifierDllName);
}

std::optional<std::wstring> ExpandEnvironmentVariables(wcstring_view str) {
  std::wstring path_expanded;
  DWORD path_len = MAX_PATH;
  for (int iterations = 0; iterations < 5; iterations++) {
    DWORD result = ::ExpandEnvironmentStringsW(
        str.c_str(), base::WriteInto(&path_expanded, path_len), path_len);
    if (!result) {
      // Failed to expand variables.
      break;
    }
    if (result <= path_len) {
      return path_expanded.substr(0, result - 1);
    }
    path_len = result;
  }

  return std::nullopt;
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

ScopedAzureADJoinStateForTesting::ScopedAzureADJoinStateForTesting(bool state)
    : initial_state_(std::exchange(*GetAzureADJoinStateStorage(), state)) {}

ScopedAzureADJoinStateForTesting::~ScopedAzureADJoinStateForTesting() {
  *GetAzureADJoinStateStorage() = initial_state_;
}

}  // namespace win
}  // namespace base
