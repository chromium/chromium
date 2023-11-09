// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_service.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/lacros_launcher.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/app_mode/force_install_observer.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/enterprise_user_session_metrics.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {
namespace {

// Whether we should skip the wait for minimum screen show time.
bool g_skip_splash_wait_for_testing = false;
bool g_block_app_launch_for_testing = false;
// Whether we should prevent Kiosk launcher from exiting when launch fails.
bool g_block_exit_on_failure_for_testing = false;
// Whether we should disable any operations using KioskProfileLoader. Used in
// tests.
bool g_disable_login_operations = false;

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
  const raw_ptr<ArcKioskAppService, ExperimentalAsh> service_;
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
      return std::make_unique<WebKioskAppServiceLauncher>(
          profile, kiosk_app_id.account_id, network_delegate);
  }
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

// Returns network name by service path.
std::string ServicePathToNetworkName(const std::string& service_path) {
  const ash::NetworkState* network =
      ash::NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network) {
    return std::string();
  }
  return network->name();
}

class DefaultNetworkMonitor : public NetworkUiController::NetworkMonitor {
 public:
  using State = NetworkStateInformer::State;
  using Observer = NetworkStateInformer::NetworkStateInformerObserver;

  DefaultNetworkMonitor()
      : network_state_informer_(base::MakeRefCounted<NetworkStateInformer>()) {
    network_state_informer_->Init();
  }

  DefaultNetworkMonitor(const DefaultNetworkMonitor&) = delete;
  DefaultNetworkMonitor& operator=(const DefaultNetworkMonitor&) = delete;
  ~DefaultNetworkMonitor() override = default;

  void AddObserver(Observer* observer) override {
    network_state_informer_->AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    network_state_informer_->RemoveObserver(observer);
  }

  State GetState() const override { return network_state_informer_->state(); }

  std::string GetNetworkName() const override {
    return ::ash::ServicePathToNetworkName(
        network_state_informer_->network_path());
  }

 private:
  scoped_refptr<NetworkStateInformer> network_state_informer_;
};

std::string ToString(app_mode::ForceInstallObserver::Result result) {
  switch (result) {
    case app_mode::ForceInstallObserver::Result::kSuccess:
      return "kSuccess";
    case app_mode::ForceInstallObserver::Result::kTimeout:
      return "kTimeout";
    case app_mode::ForceInstallObserver::Result::kInvalidPolicy:
      return "kInvalidPolicy";
  }
}

}  // namespace

using NetworkUIState = NetworkUiController::NetworkUIState;

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
                            base::BindRepeating(&BuildKioskAppLauncher),
                            std::make_unique<DefaultNetworkMonitor>()) {}

KioskLaunchController::KioskLaunchController(
    LoginDisplayHost* host,
    AppLaunchSplashScreenView* splash_screen,
    KioskAppLauncherFactory app_launcher_factory,
    std::unique_ptr<NetworkUiController::NetworkMonitor> network_monitor)
    : host_(host),
      splash_screen_view_(splash_screen),
      app_launcher_factory_(std::move(app_launcher_factory)),
      network_ui_controller_(std::make_unique<NetworkUiController>(
          *this,
          host_,
          CHECK_DEREF(splash_screen_view_.get()),
          std::move(network_monitor))) {
  if (!host_) {
    CHECK_IS_TEST();
  }
}

KioskLaunchController::~KioskLaunchController() = default;

void KioskLaunchController::Start(const KioskAppId& kiosk_app_id,
                                  bool auto_launch) {
  SYSLOG(INFO) << "Starting kiosk mode for app " << kiosk_app_id;
  kiosk_app_id_ = kiosk_app_id;
  auto_launch_ = auto_launch;
  launcher_start_time_ = base::Time::Now();

  RecordKioskLaunchUMA(auto_launch);
  SetKioskLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);

  if (host_ && host_->GetWebUILoginView()) {
    host_->GetWebUILoginView()->SetKeyboardEventsAndSystemTrayEnabled(true);
  } else if (!host_) {
    CHECK_IS_TEST();
  }

  if (auto_launch && kiosk_app_id.type == KioskAppType::kChromeApp) {
    CHECK(KioskChromeAppManager::IsInitialized());
    KioskChromeAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(
        *kiosk_app_id.app_id);
  }

  network_ui_controller_->Start();

  splash_screen_view_->Show(GetSplashScreenAppData());

  splash_wait_timer_.Start(FROM_HERE, GetSplashScreenMinTime(),
                           base::BindOnce(&KioskLaunchController::OnTimerFire,
                                          weak_ptr_factory_.GetWeakPtr()));

  if (g_disable_login_operations) {
    return;
  }

  CHECK(kiosk_app_id.account_id.is_valid());
  kiosk_profile_loader_ = std::make_unique<KioskProfileLoader>(
      kiosk_app_id_.account_id, kiosk_app_id_.type, /*delegate=*/this);
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

  const user_manager::User& user =
      CHECK_DEREF(ProfileHelper::Get()->GetUserByProfile(profile));

  if (BrowserDataMigratorImpl::MaybeRestartToMigrate(
          user.GetAccountId(), user.username_hash(),
          crosapi::browser_util::PolicyInitState::kAfterInit)) {
    LOG(WARNING) << "Restarting chrome to run profile migration.";
    return;
  }

  if (BrowserDataBackMigrator::MaybeRestartToMigrateBack(
          user.GetAccountId(), user.username_hash(),
          crosapi::browser_util::PolicyInitState::kAfterInit)) {
    LOG(WARNING) << "Restarting chrome to run backward profile migration.";
    return;
  }

  // This is needed to trigger input method extensions being loaded.
  profile->InitChromeOSPreferences();

  if (cleaned_up_) {
    LOG(WARNING) << "Profile is loaded after kiosk launch has been aborted.";
    return;
  }
  CHECK_DEREF(network_ui_controller_.get()).SetProfile(profile);

  InitializeKeyboard();
  LaunchLacros();
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

void KioskLaunchController::LaunchLacros() {
  app_state_ = kLaunchingLacros;
  lacros_launcher_ = std::make_unique<app_mode::LacrosLauncher>();
  lacros_launcher_->Start(
      base::BindOnce(&KioskLaunchController::OnLacrosLaunchComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskLaunchController::OnLacrosLaunchComplete() {
  if (network_ui_controller_->ShouldShowNetworkConfig()) {
    network_ui_controller_->UserRequestedNetworkConfig();
  } else {
    InitializeLauncher();
  }
}

void KioskLaunchController::InitializeLauncher() {
  DCHECK(!app_launcher_);

  app_state_ = kInitLauncher;
  app_launcher_ = app_launcher_factory_.Run(profile_, kiosk_app_id_,
                                            network_ui_controller_.get());
  app_launcher_observation_.Observe(app_launcher_.get());
  app_launcher_->Initialize();
}

void KioskLaunchController::OnCancelAppLaunch() {
  if (cleaned_up_) {
    return;
  }

  // Only auto-launched apps should be cancelable.
  if (KioskChromeAppManager::Get()->GetDisableBailoutShortcut() &&
      auto_launch_) {
    return;
  }

  SYSLOG(INFO) << "Canceling kiosk app launch.";

  KioskAppLaunchError::Save(KioskAppLaunchError::Error::kUserCancel);
  CleanUp();
  chrome::AttemptUserExit();
}

AppLaunchSplashScreenView::Data
KioskLaunchController::GetSplashScreenAppData() {
  absl::optional<KioskApp> app =
      KioskController::Get().GetAppById(kiosk_app_id_);
  // TODO(b/306117645) upgrade to CHECK.
  DUMP_WILL_BE_CHECK(app.has_value());

  if (!app.has_value()) {
    return AppLaunchSplashScreenView::Data(
        /*name=*/std::string(), /*icon=*/gfx::ImageSkia(), /*url=*/GURL());
  }
  return AppLaunchSplashScreenView::Data(app->name(), app->icon(),
                                         /*url=*/app->url().value_or(GURL()));
}

void KioskLaunchController::CleanUp() {
  DCHECK(!cleaned_up_);
  cleaned_up_ = true;

  splash_wait_timer_.Stop();

  splash_screen_view_ = nullptr;

  app_launcher_observation_.Reset();

  app_launcher_.reset();
  network_ui_controller_.reset();

  if (host_) {
    host_->Finalize(base::OnceClosure());
  } else {
    CHECK_IS_TEST();
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

  splash_screen_view_->Show(GetSplashScreenAppData());
}

void KioskLaunchController::OnAppPrepared() {
  SYSLOG(INFO) << "Kiosk app is ready to launch.";

  if (!splash_screen_view_) {
    return;
  }

  app_state_ = AppState::kInstallingExtensions;

  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension);
  splash_screen_view_->Show(GetSplashScreenAppData());

  force_install_observer_ = std::make_unique<app_mode::ForceInstallObserver>(
      profile_,
      base::BindOnce(&KioskLaunchController::FinishForcedExtensionsInstall,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskLaunchController::OnLaunchFailed(KioskAppLaunchError::Error error) {
  if (cleaned_up_) {
    return;
  }

  SetKioskLaunchStateCrashKey(KioskLaunchState::kLaunchFailed);

  DCHECK_NE(KioskAppLaunchError::Error::kNone, error);
  SYSLOG(ERROR) << "Kiosk launch failed, error=" << static_cast<int>(error);

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

void KioskLaunchController::FinishForcedExtensionsInstall(
    app_mode::ForceInstallObserver::Result result) {
  app_state_ = AppState::kInstalled;
  force_install_observer_.reset();

  SYSLOG(INFO) << "Kiosk finished installing extensions with result: "
               << ToString(result);

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
  splash_screen_view_->Show(GetSplashScreenAppData());

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
    splash_screen_view_->Show(GetSplashScreenAppData());
  }
  session_manager::SessionManager::Get()->SessionStarted();
}

void KioskLaunchController::OnAppWindowCreated(
    const absl::optional<std::string>& app_name) {
  SYSLOG(INFO) << "App window created, closing splash screen.";

  SetKioskLaunchStateCrashKey(KioskLaunchState::kAppWindowCreated);

  CreateKioskSystemSession(kiosk_app_id_, profile_, app_name);
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
    splash_screen_view_->Show(GetSplashScreenAppData());
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
  if (!host_) {
    CHECK_IS_TEST();
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

void KioskLaunchController::OnNetworkConfigRequested() {
  if (app_state_ == AppState::kLaunched) {
    // We do nothing since the splash screen is soon to be destroyed.
    return;
  }

  network_ui_controller_->UserRequestedNetworkConfig();
}

void KioskLaunchController::OnNetworkConfigureUiShowing() {
  splash_wait_timer_.Stop();
  app_state_ = kInitNetwork;
  launch_on_install_ = true;
  app_launcher_observation_.Reset();
  app_launcher_.reset();
}

void KioskLaunchController::OnNetworkConfigureUiFinished() {
  if (splash_screen_view_) {
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kPreparingProfile);
    splash_screen_view_->Show(GetSplashScreenAppData());
  }

  InitializeLauncher();
}

void KioskLaunchController::OnNetworkReady() {
  app_launcher_->ContinueWithNetworkReady();
}

void KioskLaunchController::OnNetworkLost() {
  if (app_state_ == kInstallingApp || app_state_ == kInstallingExtensions) {
    network_ui_controller_->OnNetworkLostDuringInstallation();
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

NetworkUiController* KioskLaunchController::GetNetworkUiControllerForTesting() {
  return network_ui_controller_.get();
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
std::unique_ptr<base::AutoReset<bool>>
KioskLaunchController::BlockAppLaunchForTesting() {
  return std::make_unique<base::AutoReset<bool>>(
      &g_block_app_launch_for_testing, true);
}

// static
base::AutoReset<bool> KioskLaunchController::BlockExitOnFailureForTesting() {
  return base::AutoReset<bool>(&g_block_exit_on_failure_for_testing, true);
}

}  // namespace ash
