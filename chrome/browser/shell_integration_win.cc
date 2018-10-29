// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration_win.h"

#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <propkey.h>  // Needs to come after shobjidl.h.
#include <stddef.h>
#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/registry.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/shlwapi.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/win/settings_app_monitor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/scoped_user_protocol_entry.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/services/util_win/public/mojom/constants.mojom.h"
#include "chrome/services/util_win/public/mojom/shell_util_win.mojom.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace shell_integration {

namespace {

// Helper function for GetAppId to generates profile id
// from profile path. "profile_id" is composed of sanitized basenames of
// user data dir and profile dir joined by a ".".
base::string16 GetProfileIdFromPath(const base::FilePath& profile_path) {
  // Return empty string if profile_path is empty
  if (profile_path.empty())
    return base::string16();

  base::FilePath default_user_data_dir;
  // Return empty string if profile_path is in default user data
  // dir and is the default profile.
  if (chrome::GetDefaultUserDataDirectory(&default_user_data_dir) &&
      profile_path.DirName() == default_user_data_dir &&
      profile_path.BaseName().value() ==
          base::ASCIIToUTF16(chrome::kInitialProfile)) {
    return base::string16();
  }

  // Get joined basenames of user data dir and profile.
  base::string16 basenames = profile_path.DirName().BaseName().value() +
      L"." + profile_path.BaseName().value();

  base::string16 profile_id;
  profile_id.reserve(basenames.size());

  // Generate profile_id from sanitized basenames.
  for (size_t i = 0; i < basenames.length(); ++i) {
    if (base::IsAsciiAlpha(basenames[i]) ||
        base::IsAsciiDigit(basenames[i]) ||
        basenames[i] == L'.')
      profile_id += basenames[i];
  }

  return profile_id;
}

base::string16 GetAppListAppName() {
  static const base::char16 kAppListAppNameSuffix[] = L"AppList";
  base::string16 app_name(install_static::GetBaseAppId());
  app_name.append(kAppListAppNameSuffix);
  return app_name;
}

// Gets expected app id for given Chrome (based on |command_line| and
// |is_per_user_install|).
base::string16 GetExpectedAppId(const base::CommandLine& command_line,
                                bool is_per_user_install) {
  base::FilePath user_data_dir;
  if (command_line.HasSwitch(switches::kUserDataDir))
    user_data_dir = command_line.GetSwitchValuePath(switches::kUserDataDir);
  else
    chrome::GetDefaultUserDataDirectory(&user_data_dir);
  // Adjust with any policy that overrides any other way to set the path.
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
  DCHECK(!user_data_dir.empty());

  base::FilePath profile_subdir;
  if (command_line.HasSwitch(switches::kProfileDirectory)) {
    profile_subdir =
        command_line.GetSwitchValuePath(switches::kProfileDirectory);
  } else {
    profile_subdir =
        base::FilePath(base::ASCIIToUTF16(chrome::kInitialProfile));
  }
  DCHECK(!profile_subdir.empty());

  base::FilePath profile_path = user_data_dir.Append(profile_subdir);
  base::string16 app_name;
  if (command_line.HasSwitch(switches::kApp)) {
    app_name = base::UTF8ToUTF16(web_app::GenerateApplicationNameFromURL(
        GURL(command_line.GetSwitchValueASCII(switches::kApp))));
  } else if (command_line.HasSwitch(switches::kAppId)) {
    app_name = base::UTF8ToUTF16(web_app::GenerateApplicationNameFromAppId(
        command_line.GetSwitchValueASCII(switches::kAppId)));
  } else if (command_line.HasSwitch(switches::kShowAppList)) {
    app_name = GetAppListAppName();
  } else {
    app_name = ShellUtil::GetBrowserModelId(is_per_user_install);
  }
  DCHECK(!app_name.empty());

  return win::GetAppModelIdForProfile(app_name, profile_path);
}

void MigrateTaskbarPinsCallback() {
  // Get full path of chrome.
  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe))
    return;

  base::FilePath pins_path;
  if (!base::PathService::Get(base::DIR_TASKBAR_PINS, &pins_path)) {
    NOTREACHED();
    return;
  }

  win::MigrateShortcutsInPathInternal(chrome_exe, pins_path);
}

// Windows treats a given scheme as an Internet scheme only if its registry
// entry has a "URL Protocol" key. Check this, otherwise we allow ProgIDs to be
// used as custom protocols which leads to security bugs.
bool IsValidCustomProtocol(const base::string16& scheme) {
  if (scheme.empty())
    return false;
  base::win::RegKey cmd_key(HKEY_CLASSES_ROOT, scheme.c_str(), KEY_QUERY_VALUE);
  return cmd_key.Valid() && cmd_key.HasValue(L"URL Protocol");
}

// Windows 8 introduced a new protocol->executable binding system which cannot
// be retrieved in the HKCR registry subkey method implemented below. We call
// AssocQueryString with the new Win8-only flag ASSOCF_IS_PROTOCOL instead.
base::string16 GetAppForProtocolUsingAssocQuery(const GURL& url) {
  const base::string16 url_scheme = base::ASCIIToUTF16(url.scheme());
  if (!IsValidCustomProtocol(url_scheme))
    return base::string16();

  // Query AssocQueryString for a human-readable description of the program
  // that will be invoked given the provided URL spec. This is used only to
  // populate the external protocol dialog box the user sees when invoking
  // an unknown external protocol.
  wchar_t out_buffer[1024];
  DWORD buffer_size = arraysize(out_buffer);
  HRESULT hr =
      AssocQueryString(ASSOCF_IS_PROTOCOL, ASSOCSTR_FRIENDLYAPPNAME,
                       url_scheme.c_str(), NULL, out_buffer, &buffer_size);
  if (FAILED(hr)) {
    DLOG(WARNING) << "AssocQueryString failed!";
    return base::string16();
  }
  return base::string16(out_buffer);
}

base::string16 GetAppForProtocolUsingRegistry(const GURL& url) {
  const base::string16 url_scheme = base::ASCIIToUTF16(url.scheme());
  if (!IsValidCustomProtocol(url_scheme))
    return base::string16();

  // First, try and extract the application's display name.
  base::string16 command_to_launch;
  base::win::RegKey cmd_key_name(HKEY_CLASSES_ROOT, url_scheme.c_str(),
                                 KEY_READ);
  if (cmd_key_name.ReadValue(NULL, &command_to_launch) == ERROR_SUCCESS &&
      !command_to_launch.empty()) {
    return command_to_launch;
  }

  // Otherwise, parse the command line in the registry, and return the basename
  // of the program path if it exists.
  const base::string16 cmd_key_path = url_scheme + L"\\shell\\open\\command";
  base::win::RegKey cmd_key_exe(HKEY_CLASSES_ROOT, cmd_key_path.c_str(),
                                KEY_READ);
  if (cmd_key_exe.ReadValue(NULL, &command_to_launch) == ERROR_SUCCESS) {
    base::CommandLine command_line(
        base::CommandLine::FromString(command_to_launch));
    return command_line.GetProgram().BaseName().value();
  }

  return base::string16();
}

DefaultWebClientState GetDefaultWebClientStateFromShellUtilDefaultState(
    ShellUtil::DefaultState default_state) {
  switch (default_state) {
    case ShellUtil::UNKNOWN_DEFAULT:
      return DefaultWebClientState::UNKNOWN_DEFAULT;
    case ShellUtil::NOT_DEFAULT:
      return DefaultWebClientState::NOT_DEFAULT;
    case ShellUtil::IS_DEFAULT:
      return DefaultWebClientState::IS_DEFAULT;
    case ShellUtil::OTHER_MODE_IS_DEFAULT:
      return DefaultWebClientState::OTHER_MODE_IS_DEFAULT;
  }
  NOTREACHED();
  return DefaultWebClientState::UNKNOWN_DEFAULT;
}

// A recorder of user actions in the Windows Settings app.
class DefaultBrowserActionRecorder : public SettingsAppMonitor::Delegate {
 public:
  // Creates the recorder and the monitor that drives it. |continuation| will be
  // run once the monitor's initialization completes (regardless of success or
  // failure).
  explicit DefaultBrowserActionRecorder(base::Closure continuation)
      : continuation_(std::move(continuation)), settings_app_monitor_(this) {}

 private:
  // win::SettingsAppMonitor::Delegate:
  void OnInitialized(HRESULT result) override {
    UMA_HISTOGRAM_BOOLEAN("SettingsAppMonitor.InitializationResult",
                          SUCCEEDED(result));
    if (SUCCEEDED(result)) {
      base::RecordAction(
          base::UserMetricsAction("SettingsAppMonitor.Initialized"));
    }
    continuation_.Run();
    continuation_ = base::Closure();
  }

  void OnAppFocused() override {
    base::RecordAction(
        base::UserMetricsAction("SettingsAppMonitor.AppFocused"));
  }

  void OnChooserInvoked() override {
    base::RecordAction(
        base::UserMetricsAction("SettingsAppMonitor.ChooserInvoked"));
  }

  void OnBrowserChosen(const base::string16& browser_name) override {
    if (browser_name == InstallUtil::GetDisplayName()) {
      base::RecordAction(
          base::UserMetricsAction("SettingsAppMonitor.ChromeBrowserChosen"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("SettingsAppMonitor.OtherBrowserChosen"));
    }
  }

  void OnPromoFocused() override {
    base::RecordAction(
        base::UserMetricsAction("SettingsAppMonitor.PromoFocused"));
  }

  void OnPromoChoiceMade(bool accept_promo) override {
    if (accept_promo) {
      base::RecordAction(
          base::UserMetricsAction("SettingsAppMonitor.CheckItOut"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("SettingsAppMonitor.SwitchAnyway"));
    }
  }

  // A closure to be run once initialization completes.
  base::Closure continuation_;

  // Monitors user interaction with the Windows Settings app for the sake of
  // reporting user actions.
  SettingsAppMonitor settings_app_monitor_;

  DISALLOW_COPY_AND_ASSIGN(DefaultBrowserActionRecorder);
};

// A function bound up in a callback with a DefaultBrowserActionRecorder and
// a closure to keep the former alive until the time comes to run the latter.
void OnSettingsAppFinished(
    std::unique_ptr<DefaultBrowserActionRecorder> recorder,
    const base::Closure& on_finished_callback) {
  recorder.reset();
  on_finished_callback.Run();
}

// There is no way to make sure the user is done with the system settings, but a
// signal that the interaction is finished is needed for UMA. A timer of 2
// minutes is used as a substitute. The registry keys for the protocol
// association with an app are also monitored to signal the end of the
// interaction early when it is clear that the user made a choice (e.g. http
// and https for default browser).
//
// This helper class manages both the timer and the registry watchers and makes
// sure the callback for the end of the settings interaction is only run once.
// This class also manages its own lifetime.
class OpenSystemSettingsHelper {
 public:
  // Begin the monitoring and will call |on_finished_callback| when done.
  // Takes in a null-terminated array of |protocols| whose registry keys must be
  // watched. The array must contain at least one element.
  static void Begin(const wchar_t* const protocols[],
                    const base::Closure& on_finished_callback) {
    delete instance_;
    instance_ = new OpenSystemSettingsHelper(protocols, on_finished_callback);
  }

 private:
  // The reason the settings interaction concluded. Do not modify the ordering
  // because it is used for UMA.
  enum ConcludeReason { REGISTRY_WATCHER, TIMEOUT, NUM_CONCLUDE_REASON_TYPES };

  OpenSystemSettingsHelper(const wchar_t* const protocols[],
                           const base::Closure& on_finished_callback)
      : scoped_user_protocol_entry_(protocols[0]),
        on_finished_callback_(on_finished_callback),
        weak_ptr_factory_(this) {
    static const wchar_t kUrlAssociationFormat[] =
        L"SOFTWARE\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\"
        L"%ls\\UserChoice";

    // Remember the start time.
    start_time_ = base::TimeTicks::Now();

    for (const wchar_t* const* scan = &protocols[0]; *scan != nullptr; ++scan) {
      AddRegistryKeyWatcher(
          base::StringPrintf(kUrlAssociationFormat, *scan).c_str());
    }
    // Only the watchers that were succesfully initialized are counted.
    registry_watcher_count_ = registry_key_watchers_.size();

    timer_.Start(
        FROM_HERE, base::TimeDelta::FromMinutes(2),
        base::Bind(&OpenSystemSettingsHelper::ConcludeInteraction,
                   weak_ptr_factory_.GetWeakPtr(), ConcludeReason::TIMEOUT));
  }

  ~OpenSystemSettingsHelper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Called when a change is detected on one of the registry keys being watched.
  // Note: All types of modification to the registry key will trigger this
  //       function even if the value change is the only one that matters. This
  //       is good enough for now.
  void OnRegistryKeyChanged() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Make sure all the registry watchers have fired.
    if (--registry_watcher_count_ == 0) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "DefaultBrowser.SettingsInteraction.RegistryWatcherDuration",
          base::TimeTicks::Now() - start_time_);

      ConcludeInteraction(ConcludeReason::REGISTRY_WATCHER);
    }
  }

  // Ends the monitoring with the system settings. Will call
  // |on_finished_callback_| and then dispose of this class instance to make
  // sure the callback won't get called subsequently.
  void ConcludeInteraction(ConcludeReason conclude_reason) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    UMA_HISTOGRAM_ENUMERATION(
        "DefaultBrowser.SettingsInteraction.ConcludeReason", conclude_reason,
        NUM_CONCLUDE_REASON_TYPES);
    on_finished_callback_.Run();
    delete instance_;
    instance_ = nullptr;
  }

  // Helper function to create a registry watcher for a given |key_path|. Do
  // nothing on initialization failure.
  void AddRegistryKeyWatcher(const wchar_t* key_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto reg_key = std::make_unique<base::win::RegKey>(HKEY_CURRENT_USER,
                                                       key_path, KEY_NOTIFY);

    if (reg_key->Valid() &&
        reg_key->StartWatching(
            base::Bind(&OpenSystemSettingsHelper::OnRegistryKeyChanged,
                       weak_ptr_factory_.GetWeakPtr()))) {
      registry_key_watchers_.push_back(std::move(reg_key));
    }
  }

  // Used to make sure only one instance is alive at the same time.
  static OpenSystemSettingsHelper* instance_;

  // This is needed to make sure that Windows displays an entry for the protocol
  // inside the "Choose default apps by protocol" settings page.
  ScopedUserProtocolEntry scoped_user_protocol_entry_;

  // The function to call when the interaction with the system settings is
  // finished.
  base::Closure on_finished_callback_;

  // The number of time the registry key watchers must fire.
  int registry_watcher_count_ = 0;

  // There can be multiple registry key watchers as some settings modify
  // multiple protocol associations. e.g. Changing the default browser modifies
  // the http and https associations.
  std::vector<std::unique_ptr<base::win::RegKey>> registry_key_watchers_;

  base::OneShotTimer timer_;

  // Records the time it takes for the final registry watcher to get signaled.
  base::TimeTicks start_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak ptrs are used to bind this class to the callbacks of the timer and the
  // registry watcher. This makes it possible to self-delete after one of the
  // callbacks is executed to cancel the remaining ones.
  base::WeakPtrFactory<OpenSystemSettingsHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenSystemSettingsHelper);
};

OpenSystemSettingsHelper* OpenSystemSettingsHelper::instance_ = nullptr;

// Helper class to determine if Chrome is pinned to the taskbar. Hides the
// complexity of managing the lifetime of the connection to the ChromeWinUtil
// service.
class IsPinnedToTaskbarHelper {
 public:
  using ResultCallback = win::IsPinnedToTaskbarCallback;
  using ErrorCallback = win::ConnectionErrorCallback;
  static void GetState(std::unique_ptr<service_manager::Connector> connector,
                       const ErrorCallback& error_callback,
                       const ResultCallback& result_callback);

 private:
  IsPinnedToTaskbarHelper(std::unique_ptr<service_manager::Connector> connector,
                          const ErrorCallback& error_callback,
                          const ResultCallback& result_callback);

  void OnConnectionError();
  void OnIsPinnedToTaskbarResult(bool succeeded, bool is_pinned_to_taskbar);

  chrome::mojom::ShellUtilWinPtr shell_util_win_ptr_;
  // The connector used to retrieve the Patch service. We can't simply use
  // content::ServiceManagerConnection::GetForProcess()->GetConnector() as this
  // is called on a background thread.
  std::unique_ptr<service_manager::Connector> connector_;

  ErrorCallback error_callback_;
  ResultCallback result_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IsPinnedToTaskbarHelper);
};

// static
void IsPinnedToTaskbarHelper::GetState(
    std::unique_ptr<service_manager::Connector> connector,
    const ErrorCallback& error_callback,
    const ResultCallback& result_callback) {
  // Self-deleting when the ShellHandler completes.
  new IsPinnedToTaskbarHelper(std::move(connector), error_callback,
                              result_callback);
}

IsPinnedToTaskbarHelper::IsPinnedToTaskbarHelper(
    std::unique_ptr<service_manager::Connector> connector,
    const ErrorCallback& error_callback,
    const ResultCallback& result_callback)
    : connector_(std::move(connector)),
      error_callback_(error_callback),
      result_callback_(result_callback) {
  DCHECK(error_callback_);
  DCHECK(result_callback_);

  connector_->BindInterface(chrome::mojom::kUtilWinServiceName,
                            &shell_util_win_ptr_);
  // |shell_util_win_ptr_| owns the callbacks and is guaranteed to be destroyed
  // before |this|, therefore making base::Unretained() safe to use.
  shell_util_win_ptr_.set_connection_error_handler(base::Bind(
      &IsPinnedToTaskbarHelper::OnConnectionError, base::Unretained(this)));
  shell_util_win_ptr_->IsPinnedToTaskbar(
      base::Bind(&IsPinnedToTaskbarHelper::OnIsPinnedToTaskbarResult,
                 base::Unretained(this)));
}

void IsPinnedToTaskbarHelper::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  error_callback_.Run();
  delete this;
}

void IsPinnedToTaskbarHelper::OnIsPinnedToTaskbarResult(
    bool succeeded,
    bool is_pinned_to_taskbar) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  result_callback_.Run(succeeded, is_pinned_to_taskbar);
  delete this;
}

}  // namespace

bool SetAsDefaultBrowser() {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    LOG(ERROR) << "Error getting app exe path";
    return false;
  }

  // From UI currently we only allow setting default browser for current user.
  if (!ShellUtil::MakeChromeDefault(ShellUtil::CURRENT_USER, chrome_exe,
                                    true /* elevate_if_not_admin */)) {
    LOG(ERROR) << "Chrome could not be set as default browser.";
    return false;
  }

  VLOG(1) << "Chrome registered as default browser.";
  return true;
}

bool SetAsDefaultProtocolClient(const std::string& protocol) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  if (protocol.empty())
    return false;

  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    LOG(ERROR) << "Error getting app exe path";
    return false;
  }

  base::string16 wprotocol(base::UTF8ToUTF16(protocol));
  if (!ShellUtil::MakeChromeDefaultProtocolClient(chrome_exe, wprotocol)) {
    LOG(ERROR) << "Chrome could not be set as default handler for "
               << protocol << ".";
    return false;
  }

  VLOG(1) << "Chrome registered as default handler for " << protocol << ".";
  return true;
}

DefaultWebClientSetPermission GetDefaultWebClientSetPermission() {
  if (!install_static::SupportsSetAsDefaultBrowser())
    return SET_DEFAULT_NOT_ALLOWED;
  if (ShellUtil::CanMakeChromeDefaultUnattended())
    return SET_DEFAULT_UNATTENDED;
  // Windows 8 and 10 both introduced a new way to set the default web client
  // which require user interaction.
  return SET_DEFAULT_INTERACTIVE;
}

bool IsElevationNeededForSettingDefaultProtocolClient() {
  return base::win::GetVersion() < base::win::VERSION_WIN8;
}

base::string16 GetApplicationNameForProtocol(const GURL& url) {
  // Windows 8 or above has a new protocol association query.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    base::string16 application_name = GetAppForProtocolUsingAssocQuery(url);
    if (!application_name.empty())
      return application_name;
  }

  return GetAppForProtocolUsingRegistry(url);
}

DefaultWebClientState GetDefaultBrowser() {
  return GetDefaultWebClientStateFromShellUtilDefaultState(
      ShellUtil::GetChromeDefaultState());
}

// There is no reliable way to say which browser is default on a machine (each
// browser can have some of the protocols/shortcuts). So we look for only HTTP
// protocol handler. Even this handler is located at different places in
// registry on XP and Vista:
// - HKCR\http\shell\open\command (XP)
// - HKCU\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\
//   http\UserChoice (Vista)
// This method checks if Firefox is default browser by checking these
// locations and returns true if Firefox traces are found there. In case of
// error (or if Firefox is not found)it returns the default value which
// is false.
bool IsFirefoxDefaultBrowser() {
  base::string16 app_cmd;
  base::win::RegKey key(HKEY_CURRENT_USER, ShellUtil::kRegVistaUrlPrefs,
                        KEY_READ);
  return key.Valid() && key.ReadValue(L"Progid", &app_cmd) == ERROR_SUCCESS &&
         app_cmd == L"FirefoxURL";
}

DefaultWebClientState IsDefaultProtocolClient(const std::string& protocol) {
  return GetDefaultWebClientStateFromShellUtilDefaultState(
      ShellUtil::GetChromeDefaultProtocolClientState(
          base::UTF8ToUTF16(protocol)));
}

namespace win {

bool SetAsDefaultBrowserUsingIntentPicker() {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    return false;
  }

  if (!ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_exe)) {
    LOG(ERROR) << "Failed to launch the set-default-browser Windows UI.";
    return false;
  }

  VLOG(1) << "Set-default-browser Windows UI completed.";
  return true;
}

void SetAsDefaultBrowserUsingSystemSettings(
    const base::Closure& on_finished_callback) {
  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    on_finished_callback.Run();
    return;
  }

  // Create an action recorder that will open the settings app once it has
  // initialized.
  std::unique_ptr<DefaultBrowserActionRecorder> recorder(
      new DefaultBrowserActionRecorder(base::Bind(
          base::IgnoreResult(&ShellUtil::ShowMakeChromeDefaultSystemUI),
          chrome_exe)));

  // The helper manages its own lifetime. Bind the action recorder
  // into the finished callback to keep it alive throughout the
  // interaction.
  static const wchar_t* const kProtocols[] = {L"http", L"https", nullptr};
  OpenSystemSettingsHelper::Begin(
      kProtocols, base::Bind(&OnSettingsAppFinished, base::Passed(&recorder),
                             on_finished_callback));
}

bool SetAsDefaultProtocolClientUsingIntentPicker(const std::string& protocol) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    return false;
  }

  base::string16 wprotocol(base::UTF8ToUTF16(protocol));
  if (!ShellUtil::ShowMakeChromeDefaultProtocolClientSystemUI(chrome_exe,
                                                              wprotocol)) {
    LOG(ERROR) << "Failed to launch the set-default-client Windows UI.";
    return false;
  }

  VLOG(1) << "Set-default-client Windows UI completed.";
  return true;
}

void SetAsDefaultProtocolClientUsingSystemSettings(
    const std::string& protocol,
    const base::Closure& on_finished_callback) {
  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    on_finished_callback.Run();
    return;
  }

  // The helper manages its own lifetime.
  base::string16 wprotocol(base::UTF8ToUTF16(protocol));
  const wchar_t* const kProtocols[] = {wprotocol.c_str(), nullptr};
  OpenSystemSettingsHelper::Begin(kProtocols, on_finished_callback);

  ShellUtil::ShowMakeChromeDefaultProtocolClientSystemUI(chrome_exe, wprotocol);
}

base::string16 GetAppModelIdForProfile(const base::string16& app_name,
                                       const base::FilePath& profile_path) {
  std::vector<base::string16> components;
  components.push_back(app_name);
  const base::string16 profile_id(GetProfileIdFromPath(profile_path));
  if (!profile_id.empty())
    components.push_back(profile_id);
  return ShellUtil::BuildAppModelId(components);
}

base::string16 GetChromiumModelIdForProfile(
    const base::FilePath& profile_path) {
  return GetAppModelIdForProfile(
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
      profile_path);
}

void MigrateTaskbarPins() {
  // This needs to happen (e.g. so that the appid is fixed and the
  // run-time Chrome icon is merged with the taskbar shortcut), but it is not an
  // urgent task.
  base::CreateCOMSTATaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::Bind(&MigrateTaskbarPinsCallback));
}

void GetIsPinnedToTaskbarState(
    std::unique_ptr<service_manager::Connector> connector,
    const ConnectionErrorCallback& on_error_callback,
    const IsPinnedToTaskbarCallback& result_callback) {
  IsPinnedToTaskbarHelper::GetState(std::move(connector), on_error_callback,
                                    result_callback);
}

int MigrateShortcutsInPathInternal(const base::FilePath& chrome_exe,
                                   const base::FilePath& path) {
  // Enumerate all pinned shortcuts in the given path directly.
  base::FileEnumerator shortcuts_enum(
      path, false,  // not recursive
      base::FileEnumerator::FILES, FILE_PATH_LITERAL("*.lnk"));

  bool is_per_user_install = InstallUtil::IsPerUserInstall();

  int shortcuts_migrated = 0;
  base::FilePath target_path;
  base::string16 arguments;
  base::win::ScopedPropVariant propvariant;
  for (base::FilePath shortcut = shortcuts_enum.Next(); !shortcut.empty();
       shortcut = shortcuts_enum.Next()) {
    // TODO(gab): Use ProgramCompare instead of comparing FilePaths below once
    // it is fixed to work with FilePaths with spaces.
    if (!base::win::ResolveShortcut(shortcut, &target_path, &arguments) ||
        chrome_exe != target_path) {
      continue;
    }
    base::CommandLine command_line(
        base::CommandLine::FromString(base::StringPrintf(
            L"\"%ls\" %ls", target_path.value().c_str(), arguments.c_str())));

    // Get the expected AppId for this Chrome shortcut.
    base::string16 expected_app_id(
        GetExpectedAppId(command_line, is_per_user_install));
    if (expected_app_id.empty())
      continue;

    // Load the shortcut.
    Microsoft::WRL::ComPtr<IShellLink> shell_link;
    Microsoft::WRL::ComPtr<IPersistFile> persist_file;
    if (FAILED(::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&shell_link))) ||
        FAILED(shell_link.CopyTo(persist_file.GetAddressOf())) ||
        FAILED(persist_file->Load(shortcut.value().c_str(), STGM_READ))) {
      DLOG(WARNING) << "Failed loading shortcut at " << shortcut.value();
      continue;
    }

    // Any properties that need to be updated on the shortcut will be stored in
    // |updated_properties|.
    base::win::ShortcutProperties updated_properties;

    // Validate the existing app id for the shortcut.
    Microsoft::WRL::ComPtr<IPropertyStore> property_store;
    propvariant.Reset();
    if (FAILED(shell_link.CopyTo(property_store.GetAddressOf())) ||
        property_store->GetValue(PKEY_AppUserModel_ID, propvariant.Receive()) !=
            S_OK) {
      // When in doubt, prefer not updating the shortcut.
      NOTREACHED();
      continue;
    } else {
      switch (propvariant.get().vt) {
        case VT_EMPTY:
          // If there is no app_id set, set our app_id if one is expected.
          if (!expected_app_id.empty())
            updated_properties.set_app_id(expected_app_id);
          break;
        case VT_LPWSTR:
          if (expected_app_id != base::string16(propvariant.get().pwszVal))
            updated_properties.set_app_id(expected_app_id);
          break;
        default:
          NOTREACHED();
          continue;
      }
    }

    // Clear dual_mode property from any shortcuts that previously had it (it
    // was only ever installed on shortcuts with the
    // |default_chromium_model_id|).
    base::string16 default_chromium_model_id(
        ShellUtil::GetBrowserModelId(is_per_user_install));
    if (expected_app_id == default_chromium_model_id) {
      propvariant.Reset();
      if (property_store->GetValue(PKEY_AppUserModel_IsDualMode,
                                   propvariant.Receive()) != S_OK) {
        // When in doubt, prefer to not update the shortcut.
        NOTREACHED();
        continue;
      }
      if (propvariant.get().vt == VT_BOOL &&
                 !!propvariant.get().boolVal) {
        updated_properties.set_dual_mode(false);
      }
    }

    persist_file.Reset();
    shell_link.Reset();

    // Update the shortcut if some of its properties need to be updated.
    if (updated_properties.options &&
        base::win::CreateOrUpdateShortcutLink(
            shortcut, updated_properties,
            base::win::SHORTCUT_UPDATE_EXISTING)) {
      ++shortcuts_migrated;
    }
  }
  return shortcuts_migrated;
}

}  // namespace win

}  // namespace shell_integration
