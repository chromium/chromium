// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_win.h"

#include <objbase.h>

#include <stdint.h>
#include <string.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/atl.h"
#include "base/win/scoped_bstr.h"
#include "base/win/win_util.h"
#include "chrome/browser/google/switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/atl_module.h"

namespace {

struct UpdateCheckResult {
  GoogleUpdateErrorCode error_code = GOOGLE_UPDATE_NO_ERROR;
  HRESULT hresult = S_OK;
};

// The status of the upgrade. These values are used for a histogram. Do not
// reorder.
enum GoogleUpdateUpgradeStatus {
  // The upgrade has started. DEPRECATED.
  // UPGRADE_STARTED = 0,
  // A check for upgrade has been initiated. DEPRECATED.
  // UPGRADE_CHECK_STARTED = 1,
  // An update is available.
  UPGRADE_IS_AVAILABLE = 2,
  // The upgrade happened successfully.
  UPGRADE_SUCCESSFUL = 3,
  // No need to upgrade, Chrome is up to date.
  UPGRADE_ALREADY_UP_TO_DATE = 4,
  // An error occurred.
  UPGRADE_ERROR = 5,
  NUM_UPGRADE_STATUS
};

GoogleUpdate3ClassFactory* g_google_update_factory = nullptr;
base::SingleThreadTaskRunner* g_update_driver_task_runner = nullptr;

// The time interval, in milliseconds, between polls to Google Update. This
// value was chosen unscientificaly during an informal discussion.
const int64_t kGoogleUpdatePollIntervalMs = 250;

const int kGoogleAllowedRetries = 1;
const int kGoogleRetryIntervalSeconds = 5;

// Constants from Google Update.
const HRESULT GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY = 0x80040813;
const HRESULT GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL = 0x8004081f;
const HRESULT GOOPDATE_E_APP_USING_EXTERNAL_UPDATER = 0xA043081D;
const HRESULT GOOPDATEINSTALL_E_INSTALLER_FAILED = 0x80040902;

// Older versions of GoogleUpdate require elevation for system-level updates.
bool IsElevationRequiredForSystemLevelUpdates() {
  const base::Version kMinGUVersionNoElevationRequired("1.3.29.1");
  const base::Version current_version(
      GoogleUpdateSettings::GetGoogleUpdateVersion(true));

  return !current_version.IsValid() ||
         current_version < kMinGUVersionNoElevationRequired;
}

// Check if the currently running instance can be updated by Google Update.
// Returns GOOGLE_UPDATE_NO_ERROR only if the instance running is a Google
// Chrome distribution installed in a standard location.
GoogleUpdateErrorCode CanUpdateCurrentChrome(
    const base::FilePath& chrome_exe_path,
    bool system_level_install) {
  DCHECK_NE(InstallUtil::IsPerUserInstall(), system_level_install);

  // The currently-running browser can only be updated by Google Update if it
  // is running from the same directory as the currently-installed browser
  // being managed by Google Update at the desired install level.
  const base::FilePath install_dir =
      installer::GetInstalledDirectory(system_level_install);
  return (!install_dir.empty() &&
          base::FilePath::CompareEqualIgnoreCase(chrome_exe_path.value(),
                                                 install_dir.value()))
             ? GOOGLE_UPDATE_NO_ERROR
             : CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY;
}

// Explicitly allow the Google Update service to impersonate the client since
// some COM code elsewhere in the browser process may have previously used
// CoInitializeSecurity to set the impersonation level to something other than
// the default. Ignore errors since an attempt to use Google Update may succeed
// regardless.
void ConfigureProxyBlanket(IUnknown* interface_pointer) {
  ::CoSetProxyBlanket(interface_pointer,
                      RPC_C_AUTHN_DEFAULT,
                      RPC_C_AUTHZ_DEFAULT,
                      COLE_DEFAULT_PRINCIPAL,
                      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                      RPC_C_IMP_LEVEL_IMPERSONATE,
                      nullptr,
                      EOAC_DYNAMIC_CLOAKING);
}

// Creates a class factory for a COM Local Server class using the Elevation
// moniker. |hwnd| must refer to a foregound window in order to get the UAC
// prompt to appear in the foreground if running on Vista+. It can also be NULL
// if background UAC prompts are desired.
HRESULT CoGetClassObjectAsAdmin(gfx::AcceleratedWidget hwnd,
                                REFCLSID class_id,
                                REFIID interface_id,
                                void** interface_ptr) {
  if (!interface_ptr)
    return E_POINTER;

  // For Vista+, need to instantiate the class factory via the elevation
  // moniker. This ensures that the UAC dialog shows up.
  const std::wstring elevation_moniker_name =
      L"Elevation:Administrator!clsid:" + base::win::WStringFromGUID(class_id);

  BIND_OPTS3 bind_opts;
  // An explicit memset is needed rather than relying on value initialization
  // since BIND_OPTS3 is not an aggregate (it is a derived type).
  memset(&bind_opts, 0, sizeof(bind_opts));
  bind_opts.cbStruct = sizeof(bind_opts);
  bind_opts.dwClassContext = CLSCTX_LOCAL_SERVER;
  bind_opts.hwnd = hwnd;

  return ::CoGetObject(elevation_moniker_name.c_str(), &bind_opts, interface_id,
                       interface_ptr);
}

HRESULT CreateGoogleUpdate3WebClass(
    bool system_level_install,
    bool install_update_if_possible,
    gfx::AcceleratedWidget elevation_window,
    Microsoft::WRL::ComPtr<IGoogleUpdate3Web>* google_update) {
  if (g_google_update_factory)
    return g_google_update_factory->Run(google_update);

  const CLSID& google_update_clsid = system_level_install
                                         ? CLSID_GoogleUpdate3WebSystemClass
                                         : CLSID_GoogleUpdate3WebUserClass;
  Microsoft::WRL::ComPtr<IClassFactory> class_factory;
  HRESULT hresult = S_OK;

  // For a user-level install, update checks and updates can both be done by a
  // normal user with the UserClass. For a system-level install, update checks
  // can be done by a normal user with the MachineClass. Newer versions of
  // GoogleUpdate allow normal users to also install system-level updates
  // without requiring elevation.
  if (!system_level_install ||
      !install_update_if_possible ||
      !IsElevationRequiredForSystemLevelUpdates()) {
    hresult = ::CoGetClassObject(google_update_clsid, CLSCTX_ALL, nullptr,
                                 IID_PPV_ARGS(&class_factory));
  } else {
    // With older versions of GoogleUpdate, a system-level update requires Admin
    // privileges. Elevate while instantiating the MachineClass.
    hresult = CoGetClassObjectAsAdmin(elevation_window, google_update_clsid,
                                      IID_PPV_ARGS(&class_factory));
  }
  if (FAILED(hresult))
    return hresult;

  ConfigureProxyBlanket(class_factory.Get());

  Microsoft::WRL::ComPtr<IUnknown> unknown;
  hresult = class_factory->CreateInstance(nullptr, IID_PPV_ARGS(&unknown));
  if (FAILED(hresult)) {
    return hresult;
  }

  // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID.
  // Without this change, marshaling can load the typelib from the wrong hive
  // (HKCU instead of HKLM, or vice-versa).
  hresult =
      unknown.CopyTo(system_level_install ? __uuidof(IGoogleUpdate3WebSystem)
                                          : __uuidof(IGoogleUpdate3WebUser),
                     IID_PPV_ARGS_Helper(&(*google_update)));
  return SUCCEEDED(hresult) ? hresult : unknown.As(&(*google_update));
}

// Returns the process-wide storage for the state of the last update check.
std::optional<UpdateState>* GetLastUpdateStateStorage() {
  static base::NoDestructor<std::optional<UpdateState>> storage;
  return storage.get();
}

// Checks if --simulate-update-hresult is present in the command line and
// returns either: nullopt if switch not present, or E_FAIL if the switch
// was present without the value, or the value of the switch as an HRESULT.
// Additionally the returned structure contains the default error code
// GOOGLE_UPDATE_ERROR_UPDATING or the value of --simulate-update-error-code.
std::optional<UpdateCheckResult> GetSimulatedErrorForDebugging() {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  if (!cmd_line.HasSwitch(switches::kSimulateUpdateHresult))
    return std::nullopt;

  uint32_t error_from_string = 0;
  std::string error_switch_value =
      cmd_line.GetSwitchValueASCII(switches::kSimulateUpdateHresult);
  HRESULT hresult = E_FAIL;
  if (base::HexStringToUInt(error_switch_value, &error_from_string))
    hresult = error_from_string;

  GoogleUpdateErrorCode error_code = GOOGLE_UPDATE_ERROR_UPDATING;
  error_switch_value =
      cmd_line.GetSwitchValueASCII(switches::kSimulateUpdateErrorCode);
  int32_t error_code_value = 0;
  if (base::StringToInt(error_switch_value, &error_code_value) &&
      error_code_value >= 0 && error_code_value < NUM_ERROR_CODES) {
    error_code = static_cast<GoogleUpdateErrorCode>(error_code_value);
  }

  return {{error_code, hresult}};
}

// UpdateCheckDriver -----------------------------------------------------------

// A driver that is created and destroyed on the caller's thread and drives
// Google Update on another.
class UpdateCheckDriver {
 public:
  UpdateCheckDriver(const UpdateCheckDriver&) = delete;
  UpdateCheckDriver& operator=(const UpdateCheckDriver&) = delete;

  // Runs an update check, invoking methods of |delegate| on the caller's thread
  // to report progress and final results.
  static void RunUpdateCheck(
      const std::string& locale,
      bool install_update_if_possible,
      gfx::AcceleratedWidget elevation_window,
      const base::WeakPtr<UpdateCheckDelegate>& delegate);

 private:
  friend class base::DeleteHelper<UpdateCheckDriver>;

  UpdateCheckDriver(
      const std::string& locale,
      bool install_update_if_possible,
      gfx::AcceleratedWidget elevation_window,
      const base::WeakPtr<UpdateCheckDelegate>& delegate);

  // Invokes a completion or error method on all delegates, as appropriate.
  ~UpdateCheckDriver();

  // If an UpdateCheckDriver is already running, the delegate is added to the
  // existing one instead of creating a new one.
  void AddDelegate(const base::WeakPtr<UpdateCheckDelegate>& delegate);

  // Notifies delegates of an update's progress. |progress|, a number between 0
  // and 100 (inclusive), is an estimation as to what percentage of the upgrade
  // has completed. |new_version| indicates the version that is being download
  // and installed.
  void NotifyUpgradeProgress(int progress, const std::u16string& new_version);

  // Starts an update check.
  void BeginUpdateCheck();

  // Returns the result of initiating an update check. On failure, the instance
  // is left in a consistent state so that this method can be invoked later to
  // retry the steps that failed.
  UpdateCheckResult BeginUpdateCheckInternal();

  // Sets status_ to UPGRADE_ERROR, update_state_.error_code to |error_code|,
  // update_state_.hresult to |check_result.hresult|,
  // update_state_.installer_exit_code to |installer_exit_code|,
  // and html_error_message_ to a composition of all values suitable for display
  // to the user. This call should be followed by deletion of the driver, which
  // will result in callers being notified via their delegates.
  void OnUpgradeError(UpdateCheckResult check_result,
                      std::optional<int> installer_exit_code,
                      const std::u16string& error_string);

  // Returns true if |current_state| and |state_value| can be obtained from the
  // ongoing update check. Otherwise, populates |hresult| with the reason they
  // could not be obtained.
  bool GetCurrentState(Microsoft::WRL::ComPtr<ICurrentState>* current_state,
                       CurrentState* state_value,
                       HRESULT* hresult) const;

  // Returns true if |current_state| and |state_value| constitute an error state
  // for the ongoing update check, in which case |error_code| is populated with
  // one of GOOGLE_UPDATE_ERROR_UPDATING, GOOGLE_UPDATE_DISABLED_BY_POLICY, or
  // GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR. |hresult| is populated with
  // the most relevant HRESULT (which may be a value from Google Update; see
  // https://code.google.com/p/omaha/source/browse/trunk/base/error.h). In case
  // Chrome's installer failed during execution, |installer_exit_code| may be
  // populated with its process exit code (see enum installer::InstallStatus in
  // chrome/installer/util/util_constants.h). |error_string| will be populated
  // with a completion message if one is provided by Google Update.
  bool IsErrorState(const Microsoft::WRL::ComPtr<ICurrentState>& current_state,
                    CurrentState state_value,
                    GoogleUpdateErrorCode* error_code,
                    HRESULT* hresult,
                    std::optional<int>* installer_exit_code,
                    std::u16string* error_string) const;

  // Returns true if |current_state| and |state_value| constitute a final state
  // for the ongoing update check, in which case |upgrade_status| is populated
  // with one of UPGRADE_ALREADY_UP_TO_DATE or UPGRADE_IS_AVAILABLE (in case a
  // pure check is being performed rather than an update) or UPGRADE_SUCCESSFUL
  // (in case an update is being performed). For the UPGRADE_IS_AVAILABLE case,
  // |new_version| will be populated with the available version, if provided by
  // Google Update.
  bool IsFinalState(const Microsoft::WRL::ComPtr<ICurrentState>& current_state,
                    CurrentState state_value,
                    GoogleUpdateUpgradeStatus* upgrade_status,
                    std::u16string* new_version) const;

  // Returns true if |current_state| and |state_value| constitute an
  // intermediate state for the ongoing update check. |new_version| will be
  // populated with the version to be installed if it is provided by Google
  // Update for the current state. |progress| will be populated with a number
  // between 0 and 100 according to how far Google Update has progressed in the
  // download and install process.
  bool IsIntermediateState(
      const Microsoft::WRL::ComPtr<ICurrentState>& current_state,
      CurrentState state_value,
      std::u16string* new_version,
      int* progress) const;

  // Polls Google Update to determine the state of the ongoing check or
  // update. If the process has reached a terminal state, this instance will be
  // deleted and the caller will be notified of the final status. Otherwise, the
  // caller will be notified of the intermediate state (iff it differs from a
  // previous notification) and another future poll will be scheduled.
  void PollGoogleUpdate();

  // The global driver instance. Accessed only on the caller's thread.
  static UpdateCheckDriver* driver_;

  // The task runner on which the update checks runs.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The caller's task runner, on which methods of the |delegates_| will be
  // invoked.
  scoped_refptr<base::SequencedTaskRunner> result_runner_;

  // The UI locale.
  std::string locale_;

  // False to only check for an update; true to also install one if available.
  bool install_update_if_possible_;

  // A parent window in case any UX is required (e.g., an elevation prompt).
  gfx::AcceleratedWidget elevation_window_;

  // Contains all delegates by which feedback is conveyed. Accessed only on the
  // caller's thread.
  std::vector<base::WeakPtr<UpdateCheckDelegate>> delegates_;

  // Number of remaining retries allowed when errors occur.
  int allowed_retries_;

  // True if operating on a per-machine installation rather than a per-user one.
  bool system_level_install_;

  // The on-demand updater that is doing the work.
  Microsoft::WRL::ComPtr<IGoogleUpdate3Web> google_update_;

  // An app bundle containing the application being updated.
  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle_;

  // The application being updated (Chrome, Chrome Binaries, or Chrome SxS).
  Microsoft::WRL::ComPtr<IAppWeb> app_;

  // The progress value reported most recently to the caller.
  int last_reported_progress_;

  // The results of the update check to be logged via UMA and/or reported to the
  // caller.
  GoogleUpdateUpgradeStatus status_;
  UpdateState update_state_;
  std::u16string html_error_message_;
};

UpdateCheckDriver* UpdateCheckDriver::driver_ = nullptr;

// static
void UpdateCheckDriver::RunUpdateCheck(
    const std::string& locale,
    bool install_update_if_possible,
    gfx::AcceleratedWidget elevation_window,
    const base::WeakPtr<UpdateCheckDelegate>& delegate) {
  // Create the driver if it doesn't exist, or add the delegate to the existing
  // one.
  if (!driver_) {
    // The driver is owned by itself, and will self-destruct when its work is
    // done.
    driver_ = new UpdateCheckDriver(locale, install_update_if_possible,
                                    elevation_window, delegate);
    driver_->task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UpdateCheckDriver::BeginUpdateCheck,
                                  base::Unretained(driver_)));
  } else {
    driver_->AddDelegate(delegate);
  }
}

// Runs on the caller's thread.
UpdateCheckDriver::UpdateCheckDriver(
    const std::string& locale,
    bool install_update_if_possible,
    gfx::AcceleratedWidget elevation_window,
    const base::WeakPtr<UpdateCheckDelegate>& delegate)
    : task_runner_(
          g_update_driver_task_runner
              ? g_update_driver_task_runner
              : base::ThreadPool::CreateCOMSTATaskRunner(
                    {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      result_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      locale_(locale),
      install_update_if_possible_(install_update_if_possible),
      elevation_window_(elevation_window),
      delegates_(1, delegate),
      allowed_retries_(kGoogleAllowedRetries),
      system_level_install_(false),
      last_reported_progress_(0),
      status_(UPGRADE_ERROR) {}

UpdateCheckDriver::~UpdateCheckDriver() {
  DCHECK(result_runner_->RunsTasksInCurrentSequence());
  // If there is an error, then error_code must not be blank, and vice versa.
  DCHECK_NE(status_ == UPGRADE_ERROR,
            update_state_.error_code == GOOGLE_UPDATE_NO_ERROR);

  *GetLastUpdateStateStorage() = update_state_;

  base::UmaHistogramEnumeration("GoogleUpdate.UpgradeResult", status_,
                                NUM_UPGRADE_STATUS);
  if (status_ == UPGRADE_ERROR) {
    base::UmaHistogramEnumeration("GoogleUpdate.UpdateErrorCode",
                                  update_state_.error_code, NUM_ERROR_CODES);
    if (FAILED(update_state_.hresult)) {
      base::UmaHistogramSparse("GoogleUpdate.ErrorHresult",
                               update_state_.hresult);
    }
  }

  // Clear the driver before calling the delegates because they might call
  // BeginUpdateCheck() and they must not add themselves to the current
  // instance of UpdateCheckDriver, which is being destroyed.
  driver_ = nullptr;

  for (const auto& delegate : delegates_) {
    if (delegate) {
      if (status_ == UPGRADE_ERROR) {
        delegate->OnError(update_state_.error_code, html_error_message_,
                          update_state_.new_version);
      } else if (install_update_if_possible_) {
        delegate->OnUpgradeComplete(update_state_.new_version);
      } else {
        delegate->OnUpdateCheckComplete(update_state_.new_version);
      }
    }
  }
}

void UpdateCheckDriver::AddDelegate(
    const base::WeakPtr<UpdateCheckDelegate>& delegate) {
  DCHECK(result_runner_->RunsTasksInCurrentSequence());
  delegates_.push_back(delegate);
}

void UpdateCheckDriver::NotifyUpgradeProgress(
    int progress,
    const std::u16string& new_version) {
  DCHECK(result_runner_->RunsTasksInCurrentSequence());

  for (const auto& delegate : delegates_) {
    if (delegate)
      delegate->OnUpgradeProgress(progress, new_version);
  }
}

void UpdateCheckDriver::BeginUpdateCheck() {
  UpdateCheckResult result = BeginUpdateCheckInternal();
  if (SUCCEEDED(result.hresult)) {
    // Start polling.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&UpdateCheckDriver::PollGoogleUpdate,
                                          base::Unretained(this)));
    return;
  }
  if (result.hresult == GOOPDATE_E_APP_USING_EXTERNAL_UPDATER) {
    // This particular transient error is worth retrying.
    if (allowed_retries_) {
      --allowed_retries_;
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&UpdateCheckDriver::BeginUpdateCheck,
                         base::Unretained(this)),
          base::Seconds(kGoogleRetryIntervalSeconds));
      return;
    }
  }

  DCHECK(FAILED(result.hresult));
  OnUpgradeError(result, std::nullopt, std::u16string());
  result_runner_->DeleteSoon(FROM_HERE, this);
}

UpdateCheckResult UpdateCheckDriver::BeginUpdateCheckInternal() {
  const auto simulated_error = GetSimulatedErrorForDebugging();
  if (simulated_error.has_value())
    return simulated_error.value();

  HRESULT hresult = S_OK;

  // Instantiate GoogleUpdate3Web{Machine,User}Class.
  if (!google_update_) {
    base::FilePath chrome_exe;
    if (!base::PathService::Get(base::DIR_EXE, &chrome_exe))
      NOTREACHED_IN_MIGRATION();

    system_level_install_ = !InstallUtil::IsPerUserInstall();

    // Make sure ATL is initialized in this module.
    ui::win::CreateATLModuleIfNeeded();

    const GoogleUpdateErrorCode error_code =
        CanUpdateCurrentChrome(chrome_exe, system_level_install_);
    if (error_code != GOOGLE_UPDATE_NO_ERROR)
      return {error_code, E_FAIL};

    hresult = CreateGoogleUpdate3WebClass(system_level_install_,
                                          install_update_if_possible_,
                                          elevation_window_, &google_update_);
    if (FAILED(hresult))
      return {GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, hresult};

    ConfigureProxyBlanket(google_update_.Get());
  }

  // The class was created, so all subsequent errors are reported as:
  constexpr GoogleUpdateErrorCode error_code =
      GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR;

  // Create an app bundle.
  if (!app_bundle_) {
    Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
    Microsoft::WRL::ComPtr<IDispatch> dispatch;
    hresult = google_update_->createAppBundleWeb(&dispatch);
    if (FAILED(hresult))
      return {error_code, hresult};

    hresult =
        dispatch.CopyTo(system_level_install_ ? __uuidof(IAppBundleWebSystem)
                                              : __uuidof(IAppBundleWebUser),
                        IID_PPV_ARGS_Helper(&app_bundle));
    if (FAILED(hresult)) {
      hresult = dispatch.As(&app_bundle);
      if (FAILED(hresult)) {
        return {error_code, hresult};
      }
    }

    dispatch.Reset();

    ConfigureProxyBlanket(app_bundle.Get());

    if (!locale_.empty()) {
      // Ignore the result of this since, while setting the display language is
      // nice to have, a failure to do so does not affect the likelihood that
      // the update check and/or install will succeed.
      app_bundle->put_displayLanguage(
          base::win::ScopedBstr(base::UTF8ToWide(locale_)).Get());
    }

    hresult = app_bundle->initialize();
    if (FAILED(hresult))
      return {error_code, hresult};
    if (elevation_window_) {
      // Likewise, a failure to set the parent window need not block an update
      // check.
      app_bundle->put_parentHWND(
          reinterpret_cast<ULONG_PTR>(elevation_window_));
    }
    app_bundle_.Swap(app_bundle);
  }

  // Get a reference to the Chrome app in the bundle.
  if (!app_) {
    const wchar_t* app_guid = install_static::GetAppGuid();
    DCHECK(app_guid);
    DCHECK(*app_guid);

    Microsoft::WRL::ComPtr<IDispatch> dispatch;
    // It is common for this call to fail with APP_USING_EXTERNAL_UPDATER if
    // an auto update is in progress.
    hresult =
        app_bundle_->createInstalledApp(base::win::ScopedBstr(app_guid).Get());
    if (FAILED(hresult))
      return {error_code, hresult};
    // Move the IAppBundleWeb reference into a local now so that failures from
    // this point onward result in it being released.
    Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
    app_bundle.Swap(app_bundle_);
    hresult = app_bundle->get_appWeb(0, &dispatch);
    if (FAILED(hresult))
      return {error_code, hresult};
    Microsoft::WRL::ComPtr<IAppWeb> app;

    // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID.
    // Without this change, marshaling can load the typelib from the wrong hive
    // (HKCU instead of HKLM, or vice-versa).
    hresult = dispatch.CopyTo(
        system_level_install_ ? __uuidof(IAppWebSystem) : __uuidof(IAppWebUser),
        IID_PPV_ARGS_Helper(&app));
    if (FAILED(hresult)) {
      hresult = dispatch.As(&app);
      if (FAILED(hresult)) {
        return {error_code, hresult};
      }
    }

    ConfigureProxyBlanket(app.Get());
    hresult = app_bundle->checkForUpdate();
    if (FAILED(hresult))
      return {error_code, hresult};
    app_bundle_.Swap(app_bundle);
    app_.Swap(app);
  }

  return {GOOGLE_UPDATE_NO_ERROR, hresult};
}

bool UpdateCheckDriver::GetCurrentState(
    Microsoft::WRL::ComPtr<ICurrentState>* current_state,
    CurrentState* state_value,
    HRESULT* hresult) const {
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  *hresult = app_->get_currentState(&dispatch);
  if (FAILED(*hresult))
    return false;

  // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID.
  // Without this change, marshaling can load the typelib from the wrong hive
  // (HKCU instead of HKLM, or vice-versa).
  *hresult =
      dispatch.CopyTo(system_level_install_ ? __uuidof(ICurrentStateSystem)
                                            : __uuidof(ICurrentStateUser),
                      IID_PPV_ARGS_Helper(&(*current_state)));
  if (FAILED(*hresult)) {
    *hresult = dispatch.As(&(*current_state));
    if (FAILED(*hresult)) {
      return false;
    }
  }
  ConfigureProxyBlanket(current_state->Get());
  LONG value = 0;
  *hresult = (*current_state)->get_stateValue(&value);
  if (FAILED(*hresult))
    return false;
  *state_value = static_cast<CurrentState>(value);
  return true;
}

bool UpdateCheckDriver::IsErrorState(
    const Microsoft::WRL::ComPtr<ICurrentState>& current_state,
    CurrentState state_value,
    GoogleUpdateErrorCode* error_code,
    HRESULT* hresult,
    std::optional<int>* installer_exit_code,
    std::u16string* error_string) const {
  if (state_value == STATE_ERROR) {
    // In general, errors reported by Google Update fall under this category
    // (see special case below).
    *error_code = GOOGLE_UPDATE_ERROR_UPDATING;

    // In general, the exit code of Chrome's installer is unknown (see special
    // case below).
    installer_exit_code->reset();

    // Report the error_code provided by Google Update if possible, or the
    // reason it wasn't possible otherwise.
    LONG long_value = 0;
    *hresult = current_state->get_errorCode(&long_value);
    if (SUCCEEDED(*hresult))
      *hresult = long_value;

    // Special cases:
    // - Use a custom error code if Google Update repoted that the update was
    //   disabled by a Group Policy setting.
    // - Extract the exit code of Chrome's installer if Google Update repoted
    //   that the update failed because of a failure in the installer.
    LONG code = 0;
    if (*hresult == GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY) {
      *error_code = GOOGLE_UPDATE_DISABLED_BY_POLICY;
    } else if (*hresult == GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL) {
      *error_code = GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY;
    } else if (*hresult == GOOPDATEINSTALL_E_INSTALLER_FAILED &&
               SUCCEEDED(current_state->get_installerResultCode(&code))) {
      *installer_exit_code = code;
    }

    base::win::ScopedBstr message;
    if (SUCCEEDED(current_state->get_completionMessage(message.Receive())))
      error_string->assign(base::as_u16cstr(message.Get()), message.Length());

    return true;
  }
  if (state_value == STATE_UPDATE_AVAILABLE && install_update_if_possible_) {
    *hresult = app_bundle_->install();
    if (FAILED(*hresult)) {
      // Report a failure to start the install as a general error while trying
      // to interact with Google Update.
      *error_code = GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR;
      installer_exit_code->reset();
      return true;
    }
    // Return false for handling in IsIntermediateState.
  }
  return false;
}

bool UpdateCheckDriver::IsFinalState(
    const Microsoft::WRL::ComPtr<ICurrentState>& current_state,
    CurrentState state_value,
    GoogleUpdateUpgradeStatus* upgrade_status,
    std::u16string* new_version) const {
  if (state_value == STATE_UPDATE_AVAILABLE && !install_update_if_possible_) {
    base::win::ScopedBstr version;
    *upgrade_status = UPGRADE_IS_AVAILABLE;
    if (SUCCEEDED(current_state->get_availableVersion(version.Receive())))
      new_version->assign(base::as_u16cstr(version.Get()), version.Length());
    return true;
  }
  if (state_value == STATE_INSTALL_COMPLETE) {
    DCHECK(install_update_if_possible_);
    *upgrade_status = UPGRADE_SUCCESSFUL;
    return true;
  }
  if (state_value == STATE_NO_UPDATE) {
    *upgrade_status = UPGRADE_ALREADY_UP_TO_DATE;
    return true;
  }
  return false;
}

bool UpdateCheckDriver::IsIntermediateState(
    const Microsoft::WRL::ComPtr<ICurrentState>& current_state,
    CurrentState state_value,
    std::u16string* new_version,
    int* progress) const {
  // ERROR will have been handled in IsErrorState. UPDATE_AVAILABLE, and
  // NO_UPDATE will have been handled in IsFinalState if not doing an install,
  // as will STATE_INSTALL_COMPLETE when doing an install. All other states
  // following UPDATE_AVAILABLE will only happen when an install is to be done.
  DCHECK(state_value < STATE_UPDATE_AVAILABLE || install_update_if_possible_);
  *progress = 0;

  switch (state_value) {
    case STATE_INIT:
    case STATE_WAITING_TO_CHECK_FOR_UPDATE:
    case STATE_CHECKING_FOR_UPDATE:
      // There is no news to report yet.
      break;

    case STATE_UPDATE_AVAILABLE: {
      base::win::ScopedBstr version;
      if (SUCCEEDED(current_state->get_availableVersion(version.Receive())))
        new_version->assign(base::as_u16cstr(version.Get()), version.Length());
      break;
    }

    case STATE_WAITING_TO_DOWNLOAD:
    case STATE_RETRYING_DOWNLOAD:
      break;

    case STATE_DOWNLOADING: {
      ULONG bytes_downloaded = 0;
      ULONG total_bytes = 0;
      if (SUCCEEDED(current_state->get_bytesDownloaded(&bytes_downloaded)) &&
          SUCCEEDED(current_state->get_totalBytesToDownload(&total_bytes)) &&
          total_bytes) {
        // 0-50 is downloading.
        *progress = base::ClampFloor((static_cast<double>(bytes_downloaded) /
                                      static_cast<double>(total_bytes)) *
                                     50.0);
      }
      break;
    }

    case STATE_DOWNLOAD_COMPLETE:
    case STATE_EXTRACTING:
    case STATE_APPLYING_DIFFERENTIAL_PATCH:
    case STATE_READY_TO_INSTALL:
    case STATE_WAITING_TO_INSTALL:
      *progress = 50;
      break;

    case STATE_INSTALLING: {
      *progress = 50;
      LONG install_progress = 0;
      if (SUCCEEDED(current_state->get_installProgress(&install_progress)) &&
          install_progress >= 0 && install_progress <= 100) {
        // 50-100 is installing.
        *progress = (50 + install_progress / 2);
      }
      break;
    }

    case STATE_INSTALL_COMPLETE:
    case STATE_PAUSED:
    case STATE_NO_UPDATE:
    case STATE_ERROR:
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  return true;
}

void UpdateCheckDriver::PollGoogleUpdate() {
  Microsoft::WRL::ComPtr<ICurrentState> state;
  CurrentState state_value = STATE_INIT;
  HRESULT hresult = S_OK;
  GoogleUpdateErrorCode error_code = GOOGLE_UPDATE_NO_ERROR;
  std::optional<int> installer_exit_code;
  std::u16string error_string;
  GoogleUpdateUpgradeStatus upgrade_status = UPGRADE_ERROR;
  std::u16string new_version;
  int progress = 0;

  if (!GetCurrentState(&state, &state_value, &hresult)) {
    OnUpgradeError({GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR, hresult},
                   std::nullopt, std::u16string());
  } else if (IsErrorState(state, state_value, &error_code, &hresult,
                          &installer_exit_code, &error_string)) {
    OnUpgradeError({error_code, hresult}, installer_exit_code, error_string);
  } else if (IsFinalState(state, state_value, &upgrade_status, &new_version)) {
    status_ = upgrade_status;
    update_state_.error_code = GOOGLE_UPDATE_NO_ERROR;
    html_error_message_.clear();
    if (!new_version.empty())
      update_state_.new_version = new_version;
    update_state_.hresult = S_OK;
    update_state_.installer_exit_code.reset();
  } else if (IsIntermediateState(state, state_value, &new_version, &progress)) {
    bool got_new_version =
        update_state_.new_version.empty() && !new_version.empty();
    if (got_new_version)
      update_state_.new_version = new_version;
    // Give the caller this status update if it differs from the last one given.
    if (got_new_version || progress != last_reported_progress_) {
      last_reported_progress_ = progress;

      // It is safe to post this task with an unretained pointer since the task
      // is guaranteed to run before a subsequent DeleteSoon is handled.
      result_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&UpdateCheckDriver::NotifyUpgradeProgress,
                         base::Unretained(this), last_reported_progress_,
                         update_state_.new_version));
    }

    // Schedule the next check.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UpdateCheckDriver::PollGoogleUpdate,
                       base::Unretained(this)),
        base::Milliseconds(kGoogleUpdatePollIntervalMs));
    // Early return for this non-terminal state.
    return;
  }

  // Release the reference on the COM objects before bouncing back to the
  // caller's thread.
  state.Reset();
  app_.Reset();
  app_bundle_.Reset();
  google_update_.Reset();

  result_runner_->DeleteSoon(FROM_HERE, this);
}

void UpdateCheckDriver::OnUpgradeError(UpdateCheckResult check_result,
                                       std::optional<int> installer_exit_code,
                                       const std::u16string& error_string) {
  status_ = UPGRADE_ERROR;
  update_state_.error_code = check_result.error_code;
  update_state_.hresult = check_result.hresult;
  update_state_.installer_exit_code = installer_exit_code;

  // Some specific result codes have dedicated messages.
  if (check_result.hresult == GOOPDATE_E_APP_USING_EXTERNAL_UPDATER) {
    html_error_message_ = l10n_util::GetStringUTF16(
        IDS_ABOUT_BOX_EXTERNAL_UPDATE_IS_RUNNING);
    return;
  }

  std::u16string html_error_msg = base::UTF8ToUTF16(base::StringPrintf(
      "%d: <a href='%s%#lX' target=_blank>%#lX</a>", update_state_.error_code,
      chrome::kUpgradeHelpCenterBaseURL, update_state_.hresult,
      update_state_.hresult));
  if (update_state_.installer_exit_code) {
    html_error_msg +=
        u": " + base::NumberToString16(*update_state_.installer_exit_code);
  }
  if (system_level_install_)
    html_error_msg += u" -- system level";
  if (error_string.empty()) {
    html_error_message_ = l10n_util::GetStringFUTF16(
        IDS_ABOUT_BOX_ERROR_UPDATE_CHECK_FAILED, html_error_msg);
  } else {
    html_error_message_ = l10n_util::GetStringFUTF16(
        IDS_ABOUT_BOX_GOOGLE_UPDATE_ERROR, error_string, html_error_msg);
  }
}

}  // namespace


// Globals ---------------------------------------------------------------------

void BeginUpdateCheck(
    const std::string& locale,
    bool install_update_if_possible,
    gfx::AcceleratedWidget elevation_window,
    const base::WeakPtr<UpdateCheckDelegate>& delegate) {
  UpdateCheckDriver::RunUpdateCheck(locale, install_update_if_possible,
                                    elevation_window, delegate);
}

// UpdateState -----------------------------------------------------------------

UpdateState::UpdateState() = default;
UpdateState::UpdateState(const UpdateState&) = default;
UpdateState::UpdateState(UpdateState&&) = default;
UpdateState& UpdateState::operator=(UpdateState&&) = default;
UpdateState::~UpdateState() = default;

std::optional<UpdateState> GetLastUpdateState() {
  return *GetLastUpdateStateStorage();
}

// Private API exposed for testing. --------------------------------------------

void SetGoogleUpdateFactoryForTesting(
    GoogleUpdate3ClassFactory google_update_factory) {
  if (g_google_update_factory) {
    delete g_google_update_factory;
    g_google_update_factory = nullptr;
  }
  if (!google_update_factory.is_null()) {
    g_google_update_factory =
        new GoogleUpdate3ClassFactory(std::move(google_update_factory));
  }
}

// TODO(calamity): Remove once a MockTimer is implemented in
// TaskEnvironment. See https://crbug.com/708584.
void SetUpdateDriverTaskRunnerForTesting(
    base::SingleThreadTaskRunner* task_runner) {
  g_update_driver_task_runner = task_runner;
}
