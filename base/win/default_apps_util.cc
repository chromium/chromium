// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/default_apps_util.h"

#include <shobjidl.h>

#include <shellapi.h>
#include <wrl/client.h>

#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"

namespace {

// Undocumented COM interface for opening the "set default app for <file type>"
// dialog.
class __declspec(uuid("6A283FE2-ECFA-4599-91C4-E80957137B26")) IOpenWithLauncher
    : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE Launch(HWND hWndParent,
                                           const wchar_t* lpszPath,
                                           int flags) = 0;
};

// Returns the class ID for the "Execute Unknown" class, read from
// `HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\OpenWith`.
// Returns std::nullopt upon failure.
std::optional<CLSID> GetOpenWithLauncherCLSID() {
  std::wstring value;
  base::win::RegKey(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OpenWith",
                    KEY_QUERY_VALUE)
      .ReadValue(L"OpenWithLauncher", &value);
  if (value.empty()) {
    return std::nullopt;
  }
  CLSID clsid;
  const auto hr = ::CLSIDFromString(value.c_str(), &clsid);
  return SUCCEEDED(hr) ? std::make_optional(clsid) : std::nullopt;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. These represent various outcomes of
// attempting to open the Settings app via the IOpenWithLauncher COM interface.
enum class OpenWithLauncherResult {
  // The settings window was launched successfully and the user changed a
  // setting.
  kSuccess = 0,
  // The settings window was launched successfully, but the user closed it
  // without taking action.
  kSuccessNoChange = 1,
  // Failed to get the class ID from the registry.
  kClsidNotFound = 2,
  // Failed to create an instance of the COM class.
  kComError = 3,
  // Launching the Settings app failed.
  kLaunchError = 4,
  kMaxValue = kLaunchError
};

// Records the `result` of opening the Settings app via the IOpenWithLauncher
// COM interface.
void RecordOpenWithLauncherResult(OpenWithLauncherResult result) {
  base::UmaHistogramEnumeration("Windows.OpenWithLauncherResult", result);
}

// Returns the target used as a activate parameter when opening the settings
// pointing to the page that is the most relevant to a user trying to change the
// default handler for `protocol`.
std::wstring GetTargetForDefaultAppsSettings(std::wstring_view protocol) {
  static constexpr std::wstring_view kSystemSettingsDefaultAppsPrefix(
      L"SystemSettings_DefaultApps_");
  if (base::EqualsCaseInsensitiveASCII(protocol, L"http")) {
    return base::StrCat({kSystemSettingsDefaultAppsPrefix, L"Browser"});
  }
  if (base::EqualsCaseInsensitiveASCII(protocol, L"mailto")) {
    return base::StrCat({kSystemSettingsDefaultAppsPrefix, L"Email"});
  }
  return L"SettingsPageAppsDefaultsProtocolView";
}

}  // namespace

namespace base::win {

bool LaunchDefaultAppsSettingsModernDialog(std::wstring_view protocol) {
  // The appModelId looks arbitrary but it is the same in Win8 and Win10. There
  // is no easy way to retrieve the appModelId from the registry.
  static constexpr wchar_t kControlPanelAppModelId[] =
      L"windows.immersivecontrolpanel_cw5n1h2txyewy"
      L"!microsoft.windows.immersivecontrolpanel";

  Microsoft::WRL::ComPtr<IApplicationActivationManager> activator;
  HRESULT hr =
      ::CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&activator));
  if (FAILED(hr)) {
    return false;
  }

  DWORD pid = 0;
  hr = activator->ActivateApplication(
      kControlPanelAppModelId, L"page=SettingsPageAppsDefaults", AO_NONE, &pid);
  if (FAILED(hr)) {
    return false;
  }
  // Scrolling to a specific protocol is only possible on Windows 10.
  if (protocol.empty() || GetVersion() >= Version::WIN11) {
    return true;
  }

  hr = activator->ActivateApplication(
      kControlPanelAppModelId,
      base::StrCat({L"page=SettingsPageAppsDefaults&target=",
                    GetTargetForDefaultAppsSettings(protocol)})
          .c_str(),
      AO_NONE, &pid);
  return SUCCEEDED(hr);
}

bool LaunchDefaultAppForFileExtensionSettings(
    base::wcstring_view file_extension,
    HWND parent_hwnd) {
  AssertComInitialized();

  // Create an "Execute Unknown" COM object with `IOpenWithLauncher` interface.
  const auto open_with_launcher_clsid = GetOpenWithLauncherCLSID();
  if (!open_with_launcher_clsid) {
    RecordOpenWithLauncherResult(OpenWithLauncherResult::kClsidNotFound);
    return false;
  }
  Microsoft::WRL::ComPtr<IOpenWithLauncher> open_with_launcher;
  if (FAILED(::CoCreateInstance(*open_with_launcher_clsid, nullptr,
                                CLSCTX_LOCAL_SERVER,
                                IID_PPV_ARGS(&open_with_launcher)))) {
    RecordOpenWithLauncherResult(OpenWithLauncherResult::kComError);
    return false;
  }

  // Open "select a default app for `file_extension` files" dialog.
  // `kOpenWithFlags` is a working `flags` argument discovered by observation.
  static constexpr int kOpenWithFlags = 0x2004;
  const HRESULT hr = open_with_launcher->Launch(
      parent_hwnd, file_extension.data(), kOpenWithFlags);
  if (SUCCEEDED(hr)) {
    RecordOpenWithLauncherResult(OpenWithLauncherResult::kSuccess);
    return true;
  }
  // On Windows 10, `ERROR_CANCELLED` just means the user closed the dialog
  // without changing anything.
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    RecordOpenWithLauncherResult(OpenWithLauncherResult::kSuccessNoChange);
    return true;
  }
  RecordOpenWithLauncherResult(OpenWithLauncherResult::kLaunchError);
  return false;
}

bool LaunchSettingsDefaultApps(std::wstring_view app_name,
                               bool is_per_user_install) {
  AssertComInitialized();

  const std::wstring settings_url = base::StrCat(
      {L"ms-settings:defaultapps?",
       is_per_user_install ? L"registeredAppUser=" : L"registeredAppMachine=",
       app_name});
  return reinterpret_cast<intptr_t>(::ShellExecute(
             /*hwnd=*/nullptr, L"open", settings_url.c_str(),
             /*lpParameters=*/nullptr,
             /*lpDirectory=*/nullptr, SW_SHOWNORMAL)) > 32;
}

bool LaunchSettingsUri(base::wcstring_view uri) {
  AssertComInitialized();

  Microsoft::WRL::ComPtr<IApplicationActivationManager> activator;
  HRESULT hr = ::CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&activator));
  if (FAILED(hr)) {
    return false;
  }

  ::CoAllowSetForegroundWindow(activator.Get(), nullptr);

  static constexpr wchar_t kControlPanelAppModelId[] =
      L"windows.immersivecontrolpanel_cw5n1h2txyewy"
      L"!microsoft.windows.immersivecontrolpanel";
  DWORD pid = 0;
  hr = activator->ActivateApplication(kControlPanelAppModelId, uri.c_str(),
                                      AO_NONE, &pid);
  return SUCCEEDED(hr);
}

}  // namespace base::win
