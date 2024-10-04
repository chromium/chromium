// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_launch_state.h"
#include "chrome/browser/ash/app_mode/kiosk_profile_load_failed_observer.h"
#include "chrome/browser/ash/app_mode/load_profile.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"
#include "chrome/browser/ash/login/app_mode/force_install_observer.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/enterprise_user_session_metrics.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "components/crash/core/common/crash_key.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user.h"
#include "url/gurl.h"

namespace ash {

using kiosk::LoadProfile;
using kiosk::LoadProfileCallback;
using kiosk::LoadProfileResult;

namespace {

// Enum types for Kiosk.LaunchType UMA so don't change its values.
// KioskLaunchType in histogram.xml must be updated when making changes here.
enum KioskLaunchType {
  KIOSK_LAUNCH_ENTERPRISE_AUTO_LAUNCH = 0,
  KIOKS_LAUNCH_ENTERPRISE_MANUAL_LAUNCH = 1,
  KIOSK_LAUNCH_CONSUMER_AUTO_LAUNCH = 2,
  KIOSK_LAUNCH_CONSUMER_MANUAL_LAUNCH = 3,
  KIOSK_LAUNCH_TYPE_COUNT  // This must be the last entry.
};

void RecordKioskLaunchUMA(bool is_auto_launch) {
  bool is_enterprise_managed =
      ash::InstallAttributes::Get()->IsEnterpriseManaged();
  const KioskLaunchType launch_type =
      is_enterprise_managed
          ? (is_auto_launch ? KIOSK_LAUNCH_ENTERPRISE_AUTO_LAUNCH
                            : KIOKS_LAUNCH_ENTERPRISE_MANUAL_LAUNCH)
          : (is_auto_launch ? KIOSK_LAUNCH_CONSUMER_AUTO_LAUNCH
                            : KIOSK_LAUNCH_CONSUMER_MANUAL_LAUNCH);

  UMA_HISTOGRAM_ENUMERATION("Kiosk.LaunchType", launch_type,
                            KIOSK_LAUNCH_TYPE_COUNT);

  if (is_enterprise_managed) {
    enterprise_user_session_metrics::RecordSignInEvent(
        is_auto_launch
            ? enterprise_user_session_metrics::SignInEventType::AUTOMATIC_KIOSK
            : enterprise_user_session_metrics::SignInEventType::MANUAL_KIOSK);
  }
}

void RecordKioskLaunchDuration(KioskAppType type, base::TimeDelta duration) {
  switch (type) {
    case KioskAppType::kChromeApp:
      base::UmaHistogramLongTimes("Kiosk.LaunchDuration.ChromeApp", duration);
      break;
    case KioskAppType::kWebApp:
      base::UmaHistogramLongTimes("Kiosk.LaunchDuration.Web", duration);
      break;
    case KioskAppType::kIsolatedWebApp:
      // TODO(crbug.com/361019026): add a separate uma value for IWA.
      NOTIMPLEMENTED();
      break;
  }
}

std::unique_ptr<KioskAppLauncher> BuildKioskAppLauncher(
    Profile* profile,
    const KioskAppId& kiosk_app_id,
    KioskAppLauncher::NetworkDelegate* network_delegate) {
  switch (kiosk_app_id.type) {
    case KioskAppType::kChromeApp:
      return std::make_unique<StartupAppLauncher>(
          profile, kiosk_app_id.app_id.value(), /*should_skip_install=*/false,
          network_delegate);
    case KioskAppType::kWebApp:
      return std::make_unique<WebKioskAppServiceLauncher>(
          profile, kiosk_app_id.account_id, network_delegate);
    case KioskAppType::kIsolatedWebApp:
      // TODO(crbug.com/361018151): impl an app service based launcher or reuse
      // WebKioskAppServiceLauncher since IWAs are installed as Web Apps.
      // Temporarily use a web kiosk as a placeholder during development.
      auto kiosk_web_apps = WebKioskAppManager::Get()->GetApps();
      CHECK_GT(kiosk_web_apps.size(), 0U);
      const WebKioskAppManager::App& placeholder_app_info = kiosk_web_apps[0];
      return std::make_unique<WebKioskAppServiceLauncher>(
          profile, placeholder_app_info.account_id, network_delegate);
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

class DefaultAcceleratorController
    : public KioskLaunchController::AcceleratorController {
 public:
  DefaultAcceleratorController() = default;
  DefaultAcceleratorController(const DefaultAcceleratorController&) = delete;
  DefaultAcceleratorController& operator=(const AcceleratorController&) =
      delete;
  ~DefaultAcceleratorController() override = default;

  void DisableAccelerators() override {
    Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
        true);
  }

  void EnableAccelerators() override {
    Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
        false);
  }
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

std::string ToString(KioskAppLaunchError::Error error) {
#define CASE(_name)                       \
  case KioskAppLaunchError::Error::_name: \
    return #_name;

  switch (error) {
    CASE(kNone);
    CASE(kHasPendingLaunch);
    CASE(kCryptohomedNotRunning);
    CASE(kAlreadyMounted);
    CASE(kUnableToMount);
    CASE(kUnableToRemove);
    CASE(kUnableToInstall);
    CASE(kUserCancel);
    CASE(kNotKioskEnabled);
    CASE(kUnableToRetrieveHash);
    CASE(kPolicyLoadFailed);
    CASE(kUnableToDownload);
    CASE(kUnableToLaunch);
    CASE(kExtensionsLoadTimeout);
    CASE(kExtensionsPolicyInvalid);
    CASE(kUserNotAllowlisted);
  }
  NOTREACHED();
#undef CASE
}

}  // namespace

// static
bool KioskLaunchController::TestOverrides::skip_splash_wait = false;
bool KioskLaunchController::TestOverrides::block_app_launch = false;
bool KioskLaunchController::TestOverrides::block_exit_on_failure = false;

using NetworkUIState = NetworkUiController::NetworkUIState;

const base::TimeDelta kDefaultKioskSplashScreenMinTime = base::Seconds(10);

class KioskLaunchController::ScopedAcceleratorDisabler {
 public:
  explicit ScopedAcceleratorDisabler(AcceleratorController& controller)
      : controller_(controller) {
    controller_->DisableAccelerators();
  }

  ScopedAcceleratorDisabler(const ScopedAcceleratorDisabler&) = delete;
  ScopedAcceleratorDisabler& operator=(const ScopedAcceleratorDisabler&) =
      delete;
  ~ScopedAcceleratorDisabler() { controller_->EnableAccelerators(); }

 private:
  raw_ref<AcceleratorController> controller_;
};

KioskLaunchController::KioskLaunchController(
    LoginDisplayHost* host,
    AppLaunchedCallback app_launched_callback,
    AppLaunchSplashScreen* splash_screen,
    LaunchCompleteCallback done_callback)
    : KioskLaunchController(
          host,
          splash_screen,
          /*profile_loader=*/base::BindOnce(&LoadProfile),
          /*app_launched_callback=*/std::move(app_launched_callback),
          /*done_callback=*/std::move(done_callback),
          /*attempt_relaunch=*/base::BindOnce(chrome::AttemptRelaunch),
          /*attempt_logout=*/base::BindOnce(chrome::AttemptUserExit),
          /*app_launcher_factory=*/base::BindRepeating(&BuildKioskAppLauncher),
          std::make_unique<DefaultNetworkMonitor>(),
          std::make_unique<DefaultAcceleratorController>()) {}

KioskLaunchController::KioskLaunchController(
    LoginDisplayHost* host,
    AppLaunchSplashScreen* splash_screen,
    LoadProfileCallback profile_loader,
    AppLaunchedCallback app_launched_callback,
    LaunchCompleteCallback done_callback,
    base::OnceClosure attempt_relaunch,
    base::OnceClosure attempt_logout,
    KioskAppLauncherFactory app_launcher_factory,
    std::unique_ptr<NetworkUiController::NetworkMonitor> network_monitor,
    std::unique_ptr<AcceleratorController> accelerator_controller)
    : host_(host),
      splash_screen_(splash_screen),
      app_launcher_factory_(std::move(app_launcher_factory)),
      network_ui_controller_(std::make_unique<NetworkUiController>(
          *this,
          host_,
          CHECK_DEREF(splash_screen_.get()),
          std::move(network_monitor))),
      app_launched_callback_(std::move(app_launched_callback)),
      done_callback_(std::move(done_callback)),
      attempt_logout_(std::move(attempt_logout)),
      attempt_relaunch_(std::move(attempt_relaunch)),
      profile_loader_(std::move(profile_loader)),
      accelerator_controller_(std::move(accelerator_controller)) {
  if (!host_) {
    CHECK_IS_TEST();
  }
}

KioskLaunchController::~KioskLaunchController() = default;

void KioskLaunchController::Start(KioskApp kiosk_app, bool auto_launch) {
  SYSLOG(INFO) << "Starting kiosk mode for app " << kiosk_app.id();
  kiosk_app_ = std::move(kiosk_app);
  auto_launch_ = auto_launch;
  launcher_start_time_ = base::Time::Now();

  RecordKioskLaunchUMA(auto_launch);
  SetKioskLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  accelerator_disabler_ =
      std::make_unique<ScopedAcceleratorDisabler>(*accelerator_controller_);

  if (host_ && host_->GetWebUILoginView()) {
    host_->GetWebUILoginView()->SetKeyboardEventsAndSystemTrayEnabled(true);
  } else if (!host_) {
    CHECK_IS_TEST();
  }

  if (auto_launch && kiosk_app_id().type == KioskAppType::kChromeApp) {
    CHECK(KioskChromeAppManager::IsInitialized());
    KioskChromeAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(
        *kiosk_app_id().app_id);
  }

  network_ui_controller_->Start();

  ShowAppLaunchSplashScreen(GetSplashScreenAppData());

  splash_wait_timer_.Start(FROM_HERE, GetSplashScreenMinTime(),
                           base::BindOnce(&KioskLaunchController::OnTimerFire,
                                          weak_ptr_factory_.GetWeakPtr()));

  CHECK(kiosk_app_id().account_id.is_valid());

  profile_loader_handle_ =
      std::move(profile_loader_)
          .Run(kiosk_app_id().account_id, kiosk_app_id().type,
               /*on_done=*/
               base::BindOnce(
                   [](KioskLaunchController* self, LoadProfileResult result) {
                     CHECK(!self->profile_) << "Kiosk profile loaded twice";
                     self->profile_loader_handle_.reset();

                     if (!result.has_value()) {
                       self->HandleProfileLoadError(std::move(result.error()));
                       return;
                     }

                     SYSLOG(INFO) << "Profile loaded... Starting app launch.";
                     self->profile_ = result.value();
                     self->StartAppLaunch(*self->profile_);
                   },
                   // Safe because `this` owns `profile_loader_handle_`.
                   base::Unretained(this)));
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

void KioskLaunchController::StartAppLaunch(Profile& profile) {
  // This is needed to trigger input method extensions being loaded.
  profile.InitChromeOSPreferences();

  if (cleaned_up_) {
    LOG(WARNING) << "Profile is loaded after kiosk launch has been aborted.";
    return;
  }
  CHECK_DEREF(network_ui_controller_.get()).SetProfile(&profile);

  InitializeKeyboard();

  if (network_ui_controller_->ShouldShowNetworkConfig()) {
    network_ui_controller_->UserRequestedNetworkConfig();
  } else {
    InitializeLauncher();
  }
}

void KioskLaunchController::InitializeKeyboard() {
  // Reset virtual keyboard to use IME engines in app profile early.
  ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();
  if (kiosk_app_id().type == KioskAppType::kWebApp) {
    // Make keyboard config sync with the `VirtualKeyboardFeatures`
    // policy.
    ChromeKeyboardControllerClient::Get()->SetKeyboardConfigFromPref(true);
  }
}

void KioskLaunchController::InitializeLauncher() {
  DCHECK(!app_launcher_);

  app_state_ = kInitLauncher;
  app_launcher_ = app_launcher_factory_.Run(profile_, kiosk_app_id(),
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

  OnLaunchFailed(KioskAppLaunchError::Error::kUserCancel);
}

AppLaunchSplashScreen::Data KioskLaunchController::GetSplashScreenAppData() {
  return AppLaunchSplashScreen::Data(
      kiosk_app().name(), kiosk_app().icon(),
      /*url=*/kiosk_app().url().value_or(GURL()));
}

void KioskLaunchController::CleanUp() {
  DCHECK(!cleaned_up_);
  cleaned_up_ = true;

  splash_wait_timer_.Stop();

  splash_screen_ = nullptr;

  app_launcher_observation_.Reset();

  app_launcher_.reset();
  network_ui_controller_.reset();
  accelerator_disabler_.reset();

  if (host_) {
    host_->Finalize(base::OnceClosure());
    host_ = nullptr;
  } else {
    CHECK_IS_TEST();
  }
  RecordKioskLaunchDuration(kiosk_app_id().type,
                            base::Time::Now() - launcher_start_time_);
}

void KioskLaunchController::OnTimerFire() {
  if (app_state_ == AppState::kLaunched) {
    FinishLaunchWithSuccess();
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
  if (!splash_screen_) {
    return;
  }

  splash_screen_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kInstallingApplication);
  UpdateSplashScreenData(GetSplashScreenAppData());
}

void KioskLaunchController::OnAppPrepared() {
  SYSLOG(INFO) << "Kiosk app is ready to launch.";

  if (!splash_screen_) {
    return;
  }

  app_state_ = AppState::kInstallingExtensions;

  splash_screen_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension);
  UpdateSplashScreenData(GetSplashScreenAppData());

  force_install_observer_ = std::make_unique<app_mode::ForceInstallObserver>(
      profile_,
      base::BindOnce(&KioskLaunchController::FinishForcedExtensionsInstall,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KioskLaunchController::OnLaunchFailed(KioskAppLaunchError::Error error) {
  using Error = KioskAppLaunchError::Error;

  if (cleaned_up_) {
    return;
  }

  SetKioskLaunchStateCrashKey(KioskLaunchState::kLaunchFailed);

  SYSLOG(ERROR) << "Kiosk launch failed, error=" << ToString(error) << " ("
                << static_cast<int>(error) << ")";

  switch (error) {
    case Error::kNone:
      NOTREACHED();
    case Error::kCryptohomedNotRunning:
    case Error::kAlreadyMounted:
      // Reboot the device on recoverable cryptohome errors. Do not save error
      // because that prevents re-launch on the next run.
      std::move(attempt_relaunch_).Run();
      break;
    case Error::kHasPendingLaunch:
    case Error::kUnableToMount:
    case Error::kUnableToRemove:
    case Error::kUnableToInstall:
    case Error::kUserCancel:
    case Error::kNotKioskEnabled:
    case Error::kUnableToRetrieveHash:
    case Error::kPolicyLoadFailed:
    case Error::kUnableToDownload:
    case Error::kUnableToLaunch:
    case Error::kExtensionsLoadTimeout:
    case Error::kExtensionsPolicyInvalid:
    case Error::kUserNotAllowlisted:
      if (KioskLaunchController::TestOverrides::block_exit_on_failure) {
        // Don't exit on launch failure if a test checks for Kiosk splash screen
        // after launch fails, which happens to MSan browser_tests since this
        // build variant runs significantly slower.
        return;
      }

      // Save the error to prevent re-launch and show the error-toast.
      KioskAppLaunchError::Save(error);
      std::move(attempt_logout_).Run();
      break;
  }

  FinishLaunchWithError(error);
}

void KioskLaunchController::FinishForcedExtensionsInstall(
    app_mode::ForceInstallObserver::Result result) {
  app_state_ = AppState::kInstalled;
  force_install_observer_.reset();

  SYSLOG(INFO) << "Kiosk finished installing extensions with result: "
               << ToString(result);

  switch (result) {
    case app_mode::ForceInstallObserver::Result::kTimeout:
      splash_screen_->ShowErrorMessage(
          KioskAppLaunchError::Error::kExtensionsLoadTimeout);
      break;
    case app_mode::ForceInstallObserver::Result::kInvalidPolicy:
      splash_screen_->ShowErrorMessage(
          KioskAppLaunchError::Error::kExtensionsPolicyInvalid);
      break;
    case app_mode::ForceInstallObserver::Result::kSuccess:
      break;
  }

  splash_screen_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow);
  UpdateSplashScreenData(GetSplashScreenAppData());

  if (launch_on_install_ || TestOverrides::skip_splash_wait) {
    LaunchApp();
  }
}

void KioskLaunchController::OnAppLaunched() {
  SYSLOG(INFO) << "Kiosk launch succeeded, wait for app window.";
  app_state_ = AppState::kLaunched;
  if (splash_screen_) {
    splash_screen_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow);
    UpdateSplashScreenData(GetSplashScreenAppData());
  }
  session_manager::SessionManager::Get()->SessionStarted();
}

void KioskLaunchController::OnAppWindowCreated(
    const std::optional<std::string>& app_name) {
  SYSLOG(INFO) << "App window created, closing splash screen.";

  app_window_name_ = app_name;

  // Not receiving the `OnAppLaunched` event before we come here leads to bugs
  // like b/335158496.
  DUMP_WILL_BE_CHECK_EQ(app_state_, AppState::kLaunched);

  SetKioskLaunchStateCrashKey(KioskLaunchState::kAppWindowCreated);
  std::move(app_launched_callback_).Run(kiosk_app_id(), profile_, app_name);

  // If timer is running, do not remove splash screen for a few
  // more seconds to give the user ability to exit kiosk session.
  if (splash_wait_timer_.IsRunning()) {
    return;
  }
  FinishLaunchWithSuccess();
}

void KioskLaunchController::OnAppDataUpdated() {
  // Updates the app title and icon in the app launch splash screen.
  UpdateSplashScreenData(GetSplashScreenAppData());
}

void KioskLaunchController::HandleProfileLoadError(
    KioskAppLaunchError::Error launch_error) {
  if (launch_error == KioskAppLaunchError::Error::kNone) {
    return;
  }

  for (auto& observer : profile_load_failed_observers_) {
    observer.OnKioskProfileLoadFailed();
  }
  OnLaunchFailed(launch_error);
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
  if (splash_screen_) {
    splash_screen_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kPreparingProfile);
    UpdateSplashScreenData(GetSplashScreenAppData());
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
  if (TestOverrides::block_app_launch) {
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

void KioskLaunchController::FinishLaunchWithSuccess() {
  CloseSplashScreen();
  std::move(done_callback_).Run(KioskAppLaunchError::Error::kNone);
}

void KioskLaunchController::FinishLaunchWithError(
    KioskAppLaunchError::Error error) {
  CloseSplashScreen();
  std::move(done_callback_).Run(error);
}

const KioskApp& KioskLaunchController::kiosk_app() const {
  CHECK(kiosk_app_.has_value());
  return kiosk_app_.value();
}

const KioskAppId& KioskLaunchController::kiosk_app_id() const {
  return kiosk_app().id();
}

void KioskLaunchController::ShowAppLaunchSplashScreen(
    AppLaunchSplashScreen::Data data) {
  UpdateSplashScreenData(std::move(data));
  if (host_) {
    host_->StartWizard(AppLaunchSplashScreenView::kScreenId);
  } else {
    CHECK_IS_TEST();
  }
}

void KioskLaunchController::UpdateSplashScreenData(
    AppLaunchSplashScreen::Data data) {
  if (splash_screen_) {
    splash_screen_->SetAppData(std::move(data));
  }
}

NetworkUiController* KioskLaunchController::GetNetworkUiControllerForTesting() {
  return network_ui_controller_.get();
}

}  // namespace ash
