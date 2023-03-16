// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_service.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/app_mode/force_install_observer.h"
#include "chrome/browser/ash/login/enterprise_user_session_metrics.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/chrome_features.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/browser/network_service_instance.h"

namespace ash {
namespace {

// Time of waiting for the network to be ready to start installation. Can be
// changed in tests.
constexpr base::TimeDelta kKioskNetworkWaitTime = base::Seconds(10);
base::TimeDelta g_network_wait_time = kKioskNetworkWaitTime;

// Whether we should skip the wait for minimum screen show time.
bool g_skip_splash_wait_for_testing = false;
bool g_block_app_launch_for_testing = false;
// Whether we should prevent Kiosk launcher from exiting when launch fails.
bool g_block_exit_on_failure_for_testing = false;
// Whether we should disable any operations using KioskProfileLoader. Used in
// tests.
bool g_disable_login_operations = false;

base::OnceClosure* network_timeout_callback = nullptr;
KioskLaunchController::ReturnBoolCallback* can_configure_network_callback =
    nullptr;
KioskLaunchController::ReturnBoolCallback*
    need_owner_auth_to_configure_network_callback = nullptr;

// Enum types for Kiosk.LaunchType UMA so don't change its values.
// KioskLaunchType in histogram.xml must be updated when making changes here.
enum KioskLaunchType {
  KIOSK_LAUNCH_ENTERPRISE_AUTO_LAUNCH = 0,
  KIOKS_LAUNCH_ENTERPRISE_MANUAL_LAUNCH = 1,
  KIOSK_LAUNCH_CONSUMER_AUTO_LAUNCH = 2,
  KIOSK_LAUNCH_CONSUMER_MANUAL_LAUNCH = 3,
  KIOSK_LAUNCH_TYPE_COUNT  // This must be the last entry.
};

bool IsDeviceEnterpriseManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->IsDeviceEnterpriseManaged();
}

void RecordKioskLaunchUMA(bool is_auto_launch) {
  const KioskLaunchType launch_type =
      IsDeviceEnterpriseManaged()
          ? (is_auto_launch ? KIOSK_LAUNCH_ENTERPRISE_AUTO_LAUNCH
                            : KIOKS_LAUNCH_ENTERPRISE_MANUAL_LAUNCH)
          : (is_auto_launch ? KIOSK_LAUNCH_CONSUMER_AUTO_LAUNCH
                            : KIOSK_LAUNCH_CONSUMER_MANUAL_LAUNCH);

  UMA_HISTOGRAM_ENUMERATION("Kiosk.LaunchType", launch_type,
                            KIOSK_LAUNCH_TYPE_COUNT);

  if (IsDeviceEnterpriseManaged()) {
    enterprise_user_session_metrics::RecordSignInEvent(
        is_auto_launch
            ? enterprise_user_session_metrics::SignInEventType::AUTOMATIC_KIOSK
            : enterprise_user_session_metrics::SignInEventType::MANUAL_KIOSK);
  }
}

void RecordKioskLaunchDuration(KioskAppType type, base::TimeDelta duration) {
  switch (type) {
    case KioskAppType::kArcApp:
      base::UmaHistogramLongTimes("Kiosk.LaunchDuration.Arc", duration);
      break;
    case KioskAppType::kChromeApp:
      base::UmaHistogramLongTimes("Kiosk.LaunchDuration.ChromeApp", duration);
      break;
    case KioskAppType::kWebApp:
      base::UmaHistogramLongTimes("Kiosk.LaunchDuration.Web", duration);
      break;
  }
}
// This is a not-owning wrapper around ArcKioskAppService which allows to be
// plugged into a unique_ptr safely.
// TODO(apotapchuk): Remove this when ARC kiosk is fully deprecated.
class ArcKioskAppServiceWrapper : public KioskAppLauncher {
 public:
  ArcKioskAppServiceWrapper(ArcKioskAppService* service,
                            KioskAppLauncher::NetworkDelegate* delegate)
      : service_(service) {
    service_->SetNetworkDelegate(delegate);
  }

  ~ArcKioskAppServiceWrapper() override {
    service_->SetNetworkDelegate(nullptr);
  }

  // `KioskAppLauncher`:
  void AddObserver(KioskAppLauncher::Observer* observer) override {
    service_->AddObserver(observer);
  }
  void RemoveObserver(KioskAppLauncher::Observer* observer) override {
    service_->RemoveObserver(observer);
  }
  void Initialize() override { service_->Initialize(); }
  void ContinueWithNetworkReady() override {
    service_->ContinueWithNetworkReady();
  }
  void LaunchApp() override { service_->LaunchApp(); }

 private:
  // `service_` is externally owned and it's the caller's responsibility to
  // ensure that it outlives this wrapper.
  ArcKioskAppService* const service_;
};

std::unique_ptr<KioskAppLauncher> BuildKioskAppLauncher(
    Profile* profile,
    const KioskAppId& kiosk_app_id,
    KioskAppLauncher::NetworkDelegate* network_delegate) {
  switch (kiosk_app_id.type) {
    case KioskAppType::kArcApp:
      // ArcKioskAppService lifetime is bound to the profile, therefore
      // wrap it into a separate object.
      return std::make_unique<ArcKioskAppServiceWrapper>(
          ArcKioskAppService::Get(profile), network_delegate);
    case KioskAppType::kChromeApp:
      return std::make_unique<StartupAppLauncher>(
          profile, kiosk_app_id.app_id.value(), /*should_skip_install=*/false,
          network_delegate);
    case KioskAppType::kWebApp:
      // TODO(b/242023891): |WebKioskAppServiceLauncher| does not support
      // Lacros until App Service installation API is available.
      if (base::FeatureList::IsEnabled(features::kKioskEnableAppService) &&
          !crosapi::browser_util::IsLacrosEnabled()) {
        return std::make_unique<WebKioskAppServiceLauncher>(
            profile, kiosk_app_id.account_id.value(), network_delegate);
      } else {
        return std::make_unique<WebKioskAppLauncher>(
            profile, kiosk_app_id.account_id.value(),
            /*should_skip_install=*/false, network_delegate);
      }
  }
  NOTREACHED();
}

base::TimeDelta GetSplashScreenMinTime() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  std::string min_time_string = command_line->GetSwitchValueASCII(
      ash::switches::kKioskSplashScreenMinTimeSeconds);

  if (min_time_string.empty()) {
    return kDefaultKioskSplashScreenMinTime;
  }

  int min_time_in_seconds;
  if (!base::StringToInt(min_time_string, &min_time_in_seconds) ||
      min_time_in_seconds < 0) {
    LOG(ERROR) << "Ignored " << ash::switches::kKioskSplashScreenMinTimeSeconds
               << "=" << min_time_string;
    return kDefaultKioskSplashScreenMinTime;
  }

  return base::Seconds(min_time_in_seconds);
}

}  // namespace

const char kKioskLaunchStateCrashKey[] = "kiosk-launch-state";
const base::TimeDelta kDefaultKioskSplashScreenMinTime = base::Seconds(10);

std::string KioskLaunchStateToString(KioskLaunchState state) {
  switch (state) {
    case KioskLaunchState::kAttemptToLaunch:
      return "attempt-to-launch";
    case KioskLaunchState::kStartLaunch:
      return "start-launch";
    case KioskLaunchState::kLauncherStarted:
      return "launcher-started";
    case KioskLaunchState::kLaunchFailed:
      return "launch-failed";
    case KioskLaunchState::kAppWindowCreated:
      return "app-window-created";
  }
}

void SetKioskLaunchStateCrashKey(KioskLaunchState state) {
  static crash_reporter::CrashKeyString<32> crash_key(
      kKioskLaunchStateCrashKey);
  crash_key.Set(KioskLaunchStateToString(state));
}

KioskLaunchController::KioskLaunchController(OobeUI* oobe_ui)
    : KioskLaunchController(LoginDisplayHost::default_host(),
                            oobe_ui->GetView<AppLaunchSplashScreenHandler>(),
                            base::BindRepeating(&BuildKioskAppLauncher)) {}

KioskLaunchController::KioskLaunchController(
    LoginDisplayHost* host,
    AppLaunchSplashScreenView* splash_screen,
    KioskAppLauncherFactory app_launcher_factory)
    : host_(host),
      splash_screen_view_(splash_screen),
      app_launcher_factory_(std::move(app_launcher_factory)) {}

KioskLaunchController::~KioskLaunchController() {
  if (splash_screen_view_) {
    splash_screen_view_->SetDelegate(nullptr);
  }
}

void KioskLaunchController::Start(const KioskAppId& kiosk_app_id,
                                  bool auto_launch) {
  SYSLOG(INFO) << "Starting kiosk mode for app " << kiosk_app_id;
  kiosk_app_id_ = kiosk_app_id;
  auto_launch_ = auto_launch;
  launcher_start_time_ = base::Time::Now();

  RecordKioskLaunchUMA(auto_launch);
  SetKioskLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);

  if (host_) {
    host_->GetLoginDisplay()->SetUIEnabled(true);
  }

  if (kiosk_app_id.type == KioskAppType::kChromeApp) {
    KioskAppManager::App app;
    DCHECK(KioskAppManager::IsInitialized());
    CHECK(KioskAppManager::Get()->GetApp(*kiosk_app_id.app_id, &app));
    kiosk_app_id_.account_id = app.account_id;
    if (auto_launch) {
      KioskAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(
          *kiosk_app_id.app_id);
    }
  }

  splash_screen_view_->SetDelegate(this);
  splash_screen_view_->Show(GetAppData());

  splash_wait_timer_.Start(FROM_HERE, GetSplashScreenMinTime(),
                           base::BindOnce(&KioskLaunchController::OnTimerFire,
                                          weak_ptr_factory_.GetWeakPtr()));

  if (g_disable_login_operations) {
    return;
  }

  kiosk_profile_loader_ = std::make_unique<KioskProfileLoader>(
      *kiosk_app_id_.account_id, kiosk_app_id_.type, /*delegate=*/this);
  kiosk_profile_loader_->Start();
}

void KioskLaunchController::AddKioskProfileLoadFailedObserver(
    KioskProfileLoadFailedObserver* observer) {
  profile_load_failed_observers_.AddObserver(observer);
}

void KioskLaunchController::RemoveKioskProfileLoadFailedObserver(
    KioskProfileLoadFailedObserver* observer) {
  profile_load_failed_observers_.RemoveObserver(observer);
}

bool KioskLaunchController::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kAppLaunchBailout) {
    OnCancelAppLaunch();
    return true;
  }

  if (action == LoginAcceleratorAction::kAppLaunchNetworkConfig) {
    OnNetworkConfigRequested();
    return true;
  }

  return false;
}

void KioskLaunchController::OnProfileLoaded(Profile* profile) {
  SYSLOG(INFO) << "Profile loaded... Starting app launch.";
  DCHECK(!profile_) << "OnProfileLoaded called twice";
  profile_ = profile;

  // Call `ClearMigrationStep()` once per signin so that the check for migration
  // is run exactly once per signin. Check the comment for `kMigrationStep` in
  // browser_data_migrator.h for details.
  BrowserDataMigratorImpl::ClearMigrationStep(g_browser_process->local_state());

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);

  // TODO(b/257210467): Remove the need for CHECK_IS_TEST
  if (!user) {
    CHECK_IS_TEST();
  } else {
    if (BrowserDataMigratorImpl::MaybeRestartToMigrate(
            user->GetAccountId(), user->username_hash(),
            crosapi::browser_util::PolicyInitState::kAfterInit)) {
      LOG(WARNING) << "Restarting chrome to run profile migration.";
      return;
    }

    if (BrowserDataBackMigrator::MaybeRestartToMigrateBack(
            user->GetAccountId(), user->username_hash(),
            crosapi::browser_util::PolicyInitState::kAfterInit)) {
      LOG(WARNING) << "Restarting chrome to run backward profile migration.";
      return;
    }
  }

  // This is needed to trigger input method extensions being loaded.
  profile->InitChromeOSPreferences();

  InitializeKeyboard();

  // We have loaded the profile, so we can create and start the
  // `KioskAppLauncher`. However, if the user has requested to configure the
  // network beforehand, we will show the dialog instead and create the launcher
  // when that's done.
  if (network_ui_state_ == NetworkUIState::kNeedToShow) {
    ShowNetworkConfigureUI();
  } else {
    InitializeLauncher();
  }
}

void KioskLaunchController::InitializeKeyboard() {
  // Reset virtual keyboard to use IME engines in app profile early.
  ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();
  if (kiosk_app_id_.type == KioskAppType::kWebApp) {
    // Make keyboard config sync with the `VirtualKeyboardFeatures`
    // policy.
    ChromeKeyboardControllerClient::Get()->SetKeyboardConfigFromPref(true);
  }
}

void KioskLaunchController::InitializeLauncher() {
  DCHECK(!app_launcher_);

  app_state_ = kInitLauncher;
  app_launcher_ = app_launcher_factory_.Run(profile_, kiosk_app_id_, this);
  app_launcher_observation_.Observe(app_launcher_.get());
  app_launcher_->Initialize();
}

void KioskLaunchController::OnConfigureNetwork() {
  DCHECK(profile_);
  if (network_ui_state_ == NetworkUIState::kShowing) {
    return;
  }

  network_ui_state_ = NetworkUIState::kShowing;
  if (CanConfigureNetwork() && NeedOwnerAuthToConfigureNetwork()) {
    host_->VerifyOwnerForKiosk(
        base::BindOnce(&KioskLaunchController::ShowNetworkConfigureUI,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // If kiosk mode was configured through enterprise policy, we may
    // not have an owner user.
    // TODO(tengs): We need to figure out the appropriate security meausres
    // for this case.
    NOTREACHED();
  }
}

void KioskLaunchController::OnCancelAppLaunch() {
  if (cleaned_up_) {
    return;
  }

  // Only auto-launched apps should be cancelable.
  if (KioskAppManager::Get()->GetDisableBailoutShortcut() && auto_launch_) {
    return;
  }

  SYSLOG(INFO) << "Canceling kiosk app launch.";

  KioskAppLaunchError::Save(KioskAppLaunchError::Error::kUserCancel);
  CleanUp();
  chrome::AttemptUserExit();
}

KioskAppManagerBase::App KioskLaunchController::GetAppData() {
  DCHECK(kiosk_app_id_.account_id.has_value());
  switch (kiosk_app_id_.type) {
    case KioskAppType::kChromeApp: {
      KioskAppManagerBase::App app;
      if (KioskAppManager::Get()->GetApp(*kiosk_app_id_.app_id, &app)) {
        return app;
      }
      break;
    }
    case KioskAppType::kArcApp: {
      const ArcKioskAppData* arc_app =
          ArcKioskAppManager::Get()->GetAppByAccountId(
              *kiosk_app_id_.account_id);
      if (arc_app) {
        return KioskAppManagerBase::App(*arc_app);
      }
      break;
    }
    case KioskAppType::kWebApp: {
      const WebKioskAppData* web_app =
          WebKioskAppManager::Get()->GetAppByAccountId(
              *kiosk_app_id_.account_id);
      if (web_app) {
        return WebKioskAppManager::CreateAppByData(*web_app);
      }
      break;
    }
  }

  LOG(WARNING) << "Cannot get a valid kiosk app. App type: "
               << (int)kiosk_app_id_.type;
  return KioskAppManagerBase::App();
}

void KioskLaunchController::CleanUp() {
  DCHECK(!cleaned_up_);
  cleaned_up_ = true;

  network_wait_timer_.Stop();
  splash_wait_timer_.Stop();

  splash_screen_view_ = nullptr;
  force_install_observer_.reset();

  kiosk_profile_loader_.reset();
  // Can be null in tests.
  if (host_) {
    host_->Finalize(base::OnceClosure());
  }
  RecordKioskLaunchDuration(kiosk_app_id_.type,
                            base::Time::Now() - launcher_start_time_);

  // Make sure that any kiosk launch errors get written to disk before we kill
  // the browser.
  g_browser_process->local_state()->CommitPendingWrite();
}

void KioskLaunchController::OnTimerFire() {
  if (app_state_ == AppState::kLaunched) {
    CloseSplashScreen();
  } else if (app_state_ == AppState::kInstalled) {
    LaunchApp();
  }
  // Always set `launch_on_install_` to true so that Kiosk launch will happen
  // immediately after retrying due to network issue.
  launch_on_install_ = true;
}

void KioskLaunchController::CloseSplashScreen() {
  if (cleaned_up_) {
    return;
  }
  CleanUp();
}

void KioskLaunchController::OnAppInstalling() {
  SYSLOG(INFO) << "Kiosk app started installing.";

  app_state_ = AppState::kInstallingApp;
  if (!splash_screen_view_) {
    return;
  }
  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kInstallingApplication);

  splash_screen_view_->Show(GetAppData());
}

void KioskLaunchController::OnAppPrepared() {
  SYSLOG(INFO) << "Kiosk app is ready to launch.";

  if (!splash_screen_view_) {
    return;
  }

  if (network_ui_state_ != NetworkUIState::kNotShowing) {
    return;
  }

  app_state_ = AppState::kInstallingExtensions;

  // Initialize and start Lacros for preparing force-installed extensions.
  if (crosapi::browser_util::IsLacrosEnabledInWebKioskSession() &&
      !crosapi::BrowserManager::Get()->IsRunningOrWillRun()) {
    SYSLOG(INFO) << "Launching lacros for web kiosk";
    crosapi::BrowserManager::Get()->InitializeAndStartIfNeeded();
  }

  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension);
  splash_screen_view_->Show(GetAppData());

  force_install_observer_ = std::make_unique<app_mode::ForceInstallObserver>(
      profile_,
      base::BindOnce(&KioskLaunchController::FinishForcedExtensionsInstall,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskLaunchController::InitializeNetwork() {
  if (!splash_screen_view_) {
    return;
  }

  network_ui_state_ = NetworkUIState::kWaitingForNetwork;
  network_wait_timer_.Start(FROM_HERE, g_network_wait_time, this,
                            &KioskLaunchController::OnNetworkWaitTimedOut);

  // When we are asked to initialize network, we should remember that this app
  // requires network.
  network_required_ = true;
  splash_screen_view_->SetNetworkRequired();

  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kPreparingNetwork);

  if (splash_screen_view_->IsNetworkReady()) {
    OnNetworkStateChanged(true);
  }
}

void KioskLaunchController::OnNetworkWaitTimedOut() {
  DCHECK(network_ui_state_ == NetworkUIState::kNotShowing ||
         network_ui_state_ == NetworkUIState::kWaitingForNetwork);

  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  content::GetNetworkConnectionTracker()->GetConnectionType(&connection_type,
                                                            base::DoNothing());
  SYSLOG(WARNING) << "OnNetworkWaitTimedout... connection = "
                  << connection_type;
  network_wait_timedout_ = true;

  MaybeShowNetworkConfigureUI();

  if (network_timeout_callback) {
    std::move(*network_timeout_callback).Run();
    network_timeout_callback = nullptr;
  }
}

bool KioskLaunchController::IsNetworkReady() const {
  return splash_screen_view_ && splash_screen_view_->IsNetworkReady();
}

bool KioskLaunchController::IsShowingNetworkConfigScreen() const {
  return network_ui_state_ == NetworkUIState::kShowing;
}

void KioskLaunchController::OnLaunchFailed(KioskAppLaunchError::Error error) {
  if (cleaned_up_) {
    return;
  }

  SetKioskLaunchStateCrashKey(KioskLaunchState::kLaunchFailed);

  DCHECK_NE(KioskAppLaunchError::Error::kNone, error);
  SYSLOG(ERROR) << "Kiosk launch failed, error=" << static_cast<int>(error);

  // App Service launcher requires the web app to be installed. Temporary issues
  // like URL redirection should not stop the app from being installed as
  // placeholder. Force launching the app is not possible in case installation
  // fails.
  if (kiosk_app_id_.type == KioskAppType::kWebApp &&
      error == KioskAppLaunchError::Error::kUnableToInstall &&
      (!base::FeatureList::IsEnabled(features::kKioskEnableAppService) ||
       crosapi::browser_util::IsLacrosEnabled())) {
    HandleWebAppInstallFailed();
    return;
  }

  // Reboot on the recoverable cryptohome errors.
  if (error == KioskAppLaunchError::Error::kCryptohomedNotRunning ||
      error == KioskAppLaunchError::Error::kAlreadyMounted) {
    // Do not save the error because saved errors would stop app from launching
    // on the next run.
    chrome::AttemptRelaunch();
    return;
  }

  // Don't exit on launch failure if a test checks for Kiosk splash screen after
  // launch fails, which happens to MSan browser_tests since this build variant
  // runs significantly slower.
  if (g_block_exit_on_failure_for_testing) {
    return;
  }

  // Saves the error and ends the session to go back to login screen.
  KioskAppLaunchError::Save(error);
  CleanUp();
  chrome::AttemptUserExit();
}

void KioskLaunchController::HandleWebAppInstallFailed() {
  // We end up here when WebKioskAppLauncher was not able to obtain metadata
  // for the app.
  // This can happen in some temporary states -- we are under captive portal, or
  // there is a third-party authorization which causes redirect to url that
  // differs from the install url. We should proceed with launch in such cases,
  // expecting this situation to not happen upon next launch.
  app_state_ = AppState::kInstalled;

  SYSLOG(WARNING) << "Failed to obtain app data, trying to launch anyway..";

  if (!splash_screen_view_) {
    return;
  }
  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          kWaitingAppWindowInstallFailed);
  splash_screen_view_->Show(GetAppData());
  if (launch_on_install_ || g_skip_splash_wait_for_testing) {
    LaunchApp();
  }
}

void KioskLaunchController::FinishForcedExtensionsInstall(
    app_mode::ForceInstallObserver::Result result) {
  app_state_ = AppState::kInstalled;
  force_install_observer_.reset();

  switch (result) {
    case app_mode::ForceInstallObserver::Result::kTimeout:
      splash_screen_view_->ShowErrorMessage(
          KioskAppLaunchError::Error::kExtensionsLoadTimeout);
      break;
    case app_mode::ForceInstallObserver::Result::kInvalidPolicy:
      splash_screen_view_->ShowErrorMessage(
          KioskAppLaunchError::Error::kExtensionsPolicyInvalid);
      break;
    case app_mode::ForceInstallObserver::Result::kSuccess:
      break;
  }

  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow);
  splash_screen_view_->Show(GetAppData());

  if (launch_on_install_ || g_skip_splash_wait_for_testing) {
    LaunchApp();
  }
}

void KioskLaunchController::OnAppLaunched() {
  SYSLOG(INFO) << "Kiosk launch succeeded, wait for app window.";
  app_state_ = AppState::kLaunched;
  if (splash_screen_view_) {
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow);
    splash_screen_view_->Show(GetAppData());
  }
  session_manager::SessionManager::Get()->SessionStarted();
}

void KioskLaunchController::OnAppWindowCreated(
    const absl::optional<std::string>& app_name) {
  SYSLOG(INFO) << "App window created, closing splash screen.";

  SetKioskLaunchStateCrashKey(KioskLaunchState::kAppWindowCreated);

  CreateAppSession(kiosk_app_id_, profile_, app_name);
  // If timer is running, do not remove splash screen for a few
  // more seconds to give the user ability to exit kiosk session.
  if (splash_wait_timer_.IsRunning()) {
    return;
  }
  CloseSplashScreen();
}

void KioskLaunchController::OnAppDataUpdated() {
  // Invokes Show() to update the app title and icon.
  if (splash_screen_view_) {
    splash_screen_view_->Show(GetAppData());
  }
}

void KioskLaunchController::OnProfileLoadFailed(
    KioskAppLaunchError::Error error) {
  for (auto& observer : profile_load_failed_observers_) {
    observer.OnKioskProfileLoadFailed();
  }
  OnLaunchFailed(error);
}

void KioskLaunchController::OnOldEncryptionDetected(
    std::unique_ptr<UserContext> user_context) {
  if (kiosk_app_id_.type != KioskAppType::kArcApp) {
    NOTREACHED();
    return;
  }
  host_->StartWizard(EncryptionMigrationScreenView::kScreenId);
  EncryptionMigrationScreen* migration_screen =
      static_cast<EncryptionMigrationScreen*>(
          host_->GetWizardController()->current_screen());
  DCHECK(migration_screen);
  migration_screen->SetUserContext(std::move(user_context));
  migration_screen->SetupInitialView();
}

bool KioskLaunchController::CanConfigureNetwork() {
  if (can_configure_network_callback) {
    return can_configure_network_callback->Run();
  }

  if (IsDeviceEnterpriseManaged()) {
    bool should_prompt;
    if (CrosSettings::Get()->GetBoolean(
            kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline,
            &should_prompt)) {
      return should_prompt;
    }
    // Default to true to allow network configuration if the policy is
    // missing.
    return true;
  }

  return user_manager::UserManager::Get()->GetOwnerAccountId().is_valid();
}

bool KioskLaunchController::NeedOwnerAuthToConfigureNetwork() {
  if (need_owner_auth_to_configure_network_callback) {
    return need_owner_auth_to_configure_network_callback->Run();
  }

  return !IsDeviceEnterpriseManaged();
}

void KioskLaunchController::MaybeShowNetworkConfigureUI() {
  SYSLOG(INFO) << "Network configure UI was requested to be shown.";
  if (!splash_screen_view_) {
    return;
  }

  if (!profile_) {
    SYSLOG(INFO)
        << "Postponing configure network dialog till profile is loaded.";
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kShowingNetworkConfigureUI);
    network_ui_state_ = kNeedToShow;
    return;
  }

  if (CanConfigureNetwork()) {
    if (NeedOwnerAuthToConfigureNetwork()) {
      if (!network_wait_timedout_) {
        OnConfigureNetwork();
      } else {
        splash_screen_view_->ToggleNetworkConfig(true);
      }
    } else {
      ShowNetworkConfigureUI();
    }
  } else {
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kNetworkWaitTimeout);
  }
}

void KioskLaunchController::ShowNetworkConfigureUI() {
  DCHECK(profile_);

  // We're about to show the network configure UI, so we destroy the
  // app_launcher_ and effectively reset the installation state. A new launcher
  // will be created in `OnNetworkConfigFinished`.
  app_launcher_observation_.Reset();
  app_launcher_.reset();
  // We should stop timers since they may fire during network
  // configure UI.
  splash_wait_timer_.Stop();
  network_wait_timer_.Stop();
  launch_on_install_ = true;
  app_state_ = kInitNetwork;
  network_ui_state_ = NetworkUIState::kShowing;
  splash_screen_view_->ShowNetworkConfigureUI();
}

void KioskLaunchController::CloseNetworkConfigureScreenIfOnline() {
  if (network_ui_state_ == NetworkUIState::kShowing && network_wait_timedout_) {
    SYSLOG(INFO) << "We are back online, closing network configure screen.";
    splash_screen_view_->ToggleNetworkConfig(false);
    splash_screen_view_->ContinueAppLaunch();
    network_ui_state_ = NetworkUIState::kNotShowing;
  }
}

void KioskLaunchController::OnNetworkConfigRequested() {
  if (app_state_ == kLaunched) {
    // We do nothing since the splash screen is soon to be destroyed.
    return;
  }

  MaybeShowNetworkConfigureUI();
}

void KioskLaunchController::OnNetworkConfigFinished() {
  network_ui_state_ = NetworkUIState::kNotShowing;

  if (splash_screen_view_) {
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kPreparingProfile);
    splash_screen_view_->Show(GetAppData());
  }

  InitializeLauncher();
}

void KioskLaunchController::OnNetworkStateChanged(bool online) {
  if (online) {
    OnNetworkOnline();
  } else {
    OnNetworkOffline();
  }
}

void KioskLaunchController::OnNetworkOnline() {
  bool network_showing_after_timeout =
      network_wait_timedout_ && network_ui_state_ == NetworkUIState::kShowing;

  if (!network_showing_after_timeout &&
      network_ui_state_ != NetworkUIState::kWaitingForNetwork) {
    // The UI is not showing at all, or was requested by the user so we do
    // nothing
    return;
  }

  // Close the network UI and continue the app launch
  network_wait_timer_.Stop();
  CloseNetworkConfigureScreenIfOnline();
  network_ui_state_ = kNotShowing;
  if (app_launcher_) {
    app_launcher_->ContinueWithNetworkReady();
  }
}

void KioskLaunchController::OnNetworkOffline() {
  if (network_required_ && (app_state_ == AppState::kInstallingApp ||
                            app_state_ == AppState::kInstallingExtensions)) {
    SYSLOG(WARNING)
        << "Connection lost during installation, restarting launcher.";
    OnNetworkWaitTimedOut();
  }
}

void KioskLaunchController::LaunchApp() {
  if (g_block_app_launch_for_testing) {
    return;
  }

  DCHECK(app_state_ == AppState::kInstalled);
  // We need to change the session state so we are able to create browser
  // windows.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  splash_wait_timer_.Stop();
  app_launcher_->LaunchApp();
}

// static
std::unique_ptr<base::AutoReset<bool>>
KioskLaunchController::DisableLoginOperationsForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&g_disable_login_operations,
                                                 true);
}

// static
std::unique_ptr<base::AutoReset<bool>>
KioskLaunchController::SkipSplashScreenWaitForTesting() {
  return std::make_unique<base::AutoReset<bool>>(
      &g_skip_splash_wait_for_testing, true);
}

// static
std::unique_ptr<base::AutoReset<base::TimeDelta>>
KioskLaunchController::SetNetworkWaitForTesting(base::TimeDelta wait_time) {
  return std::make_unique<base::AutoReset<base::TimeDelta>>(
      &g_network_wait_time, wait_time);
}

// static
std::unique_ptr<base::AutoReset<bool>>
KioskLaunchController::BlockAppLaunchForTesting() {
  return std::make_unique<base::AutoReset<bool>>(
      &g_block_app_launch_for_testing, true);
}

// static
base::AutoReset<bool> KioskLaunchController::BlockExitOnFailureForTesting() {
  return base::AutoReset<bool>(&g_block_exit_on_failure_for_testing, true);
}

// static
void KioskLaunchController::SetNetworkTimeoutCallbackForTesting(
    base::OnceClosure* callback) {
  network_timeout_callback = callback;
}

// static
void KioskLaunchController::SetCanConfigureNetworkCallbackForTesting(
    ReturnBoolCallback* callback) {
  can_configure_network_callback = callback;
}

// static
void KioskLaunchController::
    SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
        ReturnBoolCallback* callback) {
  need_owner_auth_to_configure_network_callback = callback;
}

}  // namespace ash
