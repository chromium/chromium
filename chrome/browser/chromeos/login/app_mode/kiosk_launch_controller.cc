// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/app_mode/kiosk_launch_controller.h"

#include "base/metrics/histogram_macros.h"
#include "base/syslog_logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_launcher.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/enterprise_user_session_metrics.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/public/browser/network_service_instance.h"

namespace chromeos {

namespace {
// Web Kiosk splash screen minimum show time.
constexpr base::TimeDelta kKioskSplashScreenMinTime =
    base::TimeDelta::FromSeconds(10);

// Time of waiting for the network to be ready to start installation. Can be
// changed in tests.
constexpr base::TimeDelta kKioskNetworkWaitTime =
    base::TimeDelta::FromSeconds(10);
base::TimeDelta g_network_wait_time = kKioskNetworkWaitTime;

// Whether we should skip the wait for minimum screen show time.
bool g_skip_splash_wait_for_testing = false;
bool g_block_app_launch_for_testing = false;
// Whether we should disable splash wait timer and do not perform any operations
// using KioskProfileLoader. Used in tests.
bool g_disable_wait_timer_and_login_operations = false;

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

bool IsEnterpriseManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->IsEnterpriseManaged();
}

void RecordKioskLaunchUMA(bool is_auto_launch) {
  const KioskLaunchType launch_type =
      IsEnterpriseManaged()
          ? (is_auto_launch ? KIOSK_LAUNCH_ENTERPRISE_AUTO_LAUNCH
                            : KIOKS_LAUNCH_ENTERPRISE_MANUAL_LAUNCH)
          : (is_auto_launch ? KIOSK_LAUNCH_CONSUMER_AUTO_LAUNCH
                            : KIOSK_LAUNCH_CONSUMER_MANUAL_LAUNCH);

  UMA_HISTOGRAM_ENUMERATION("Kiosk.LaunchType", launch_type,
                            KIOSK_LAUNCH_TYPE_COUNT);

  if (IsEnterpriseManaged()) {
    enterprise_user_session_metrics::RecordSignInEvent(
        is_auto_launch
            ? enterprise_user_session_metrics::SignInEventType::AUTOMATIC_KIOSK
            : enterprise_user_session_metrics::SignInEventType::MANUAL_KIOSK);
  }
}

// This is a not-owning wrapper around ArcKioskAppService which allows to be
// plugged into a unique_ptr safely.
// TODO(apotapchuk): Remove this when ARC kiosk is fully deprecated.
class ArcKioskAppServiceWrapper : public KioskAppLauncher {
 public:
  ArcKioskAppServiceWrapper(ArcKioskAppService* service,
                            KioskAppLauncher::Delegate* delegate)
      : service_(service) {
    service_->SetDelegate(delegate);
  }

  ~ArcKioskAppServiceWrapper() override { service_->SetDelegate(nullptr); }

  // KioskAppLauncher:
  void Initialize() override { service_->Initialize(); }
  void ContinueWithNetworkReady() override {
    service_->ContinueWithNetworkReady();
  }
  void RestartLauncher() override { service_->RestartLauncher(); }
  void LaunchApp() override { service_->LaunchApp(); }

 private:
  ArcKioskAppService* const service_;
};

}  // namespace

KioskLaunchController::KioskLaunchController(OobeUI* oobe_ui)
    : host_(LoginDisplayHost::default_host()),
      oobe_ui_(oobe_ui),
      splash_screen_view_(oobe_ui->GetView<AppLaunchSplashScreenHandler>()) {}

KioskLaunchController::KioskLaunchController() : host_(nullptr) {}

KioskLaunchController::~KioskLaunchController() {
  if (splash_screen_view_)
    splash_screen_view_->SetDelegate(nullptr);
}

void KioskLaunchController::Start(const KioskAppId& kiosk_app_id,
                                  bool auto_launch) {
  SYSLOG(INFO) << "Starting kiosk mode of type "
               << static_cast<int>(kiosk_app_id.type) << "...";
  kiosk_app_id_ = kiosk_app_id;

  RecordKioskLaunchUMA(auto_launch);

  if (host_)
    host_->GetLoginDisplay()->SetUIEnabled(true);

  if (kiosk_app_id.type == KioskAppType::CHROME_APP) {
    KioskAppManager::App app;
    CHECK(KioskAppManager::Get());
    CHECK(KioskAppManager::Get()->GetApp(*kiosk_app_id.app_id, &app));
    kiosk_app_id_.account_id = app.account_id;
    if (auto_launch)
      KioskAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(
          *kiosk_app_id.app_id);
  }

  splash_screen_view_->SetDelegate(this);
  splash_screen_view_->Show();

  if (g_disable_wait_timer_and_login_operations)
    return;

  splash_wait_timer_.Start(FROM_HERE, kKioskSplashScreenMinTime,
                           base::BindOnce(&KioskLaunchController::OnTimerFire,
                                          weak_ptr_factory_.GetWeakPtr()));

  kiosk_profile_loader_.reset(
      new KioskProfileLoader(*kiosk_app_id_.account_id, kiosk_app_id_.type,
                             /*use_guest_mount=*/false, /*delegate=*/this));
  kiosk_profile_loader_->Start();
}

void KioskLaunchController::OnProfileLoaded(Profile* profile) {
  SYSLOG(INFO) << "Profile loaded... Starting app launch.";
  profile_ = profile;

  // This is needed to trigger input method extensions being loaded.
  profile->InitChromeOSPreferences();

  // Reset virtual keyboard to use IME engines in app profile early.
  ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();

  // Do not set update |app_launcher_| if has been set.
  if (!app_launcher_) {
    switch (kiosk_app_id_.type) {
      case KioskAppType::ARC_APP:
        // ArcKioskAppService lifetime is bound to the profile, therefore
        // wrap it into a separate object.
        app_launcher_ = std::make_unique<ArcKioskAppServiceWrapper>(
            ArcKioskAppService::Get(profile_), this);
        break;
      case KioskAppType::CHROME_APP:
        app_launcher_ = std::make_unique<StartupAppLauncher>(
            profile_, *kiosk_app_id_.app_id, this);
        break;
      case KioskAppType::WEB_APP:
        // Make keyboard config sync with the |VirtualKeyboardFeatures| policy.
        ChromeKeyboardControllerClient::Get()->SetKeyboardConfigFromPref(true);
        app_launcher_ = std::make_unique<WebKioskAppLauncher>(
            profile, this, *kiosk_app_id_.account_id);
        break;
    }
  }

  app_launcher_->Initialize();
  if (network_ui_state_ == NetworkUIState::NEED_TO_SHOW)
    ShowNetworkConfigureUI();
}

void KioskLaunchController::OnConfigureNetwork() {
  DCHECK(profile_);
  if (network_ui_state_ == NetworkUIState::SHOWING)
    return;

  network_ui_state_ = NetworkUIState::SHOWING;
  if (CanConfigureNetwork() && NeedOwnerAuthToConfigureNetwork()) {
    signin_screen_.reset(new AppLaunchSigninScreen(oobe_ui_, this));
    signin_screen_->Show();
  } else {
    // If kiosk mode was configured through enterprise policy, we may
    // not have an owner user.
    // TODO(tengs): We need to figure out the appropriate security meausres
    // for this case.
    NOTREACHED();
  }
}

void KioskLaunchController::OnCancelAppLaunch() {
  if (KioskAppManager::Get()->GetDisableBailoutShortcut())
    return;

  SYSLOG(INFO) << "Canceling kiosk app launch.";

  KioskAppLaunchError::Save(KioskAppLaunchError::USER_CANCEL);
  CleanUp();
  chrome::AttemptUserExit();
}

void KioskLaunchController::OnDeletingSplashScreenView() {
  splash_screen_view_ = nullptr;
}

KioskAppManagerBase::App KioskLaunchController::GetAppData() {
  DCHECK(kiosk_app_id_.account_id.has_value());
  switch (kiosk_app_id_.type) {
    case KioskAppType::CHROME_APP: {
      KioskAppManagerBase::App app;
      bool app_found =
          KioskAppManager::Get()->GetApp(*kiosk_app_id_.app_id, &app);
      DCHECK(app_found);
      return app;
    }
    case KioskAppType::ARC_APP: {
      const ArcKioskAppData* arc_app =
          ArcKioskAppManager::Get()->GetAppByAccountId(
              *kiosk_app_id_.account_id);
      DCHECK(arc_app);
      return KioskAppManagerBase::App(*arc_app);
    }
    case KioskAppType::WEB_APP: {
      const WebKioskAppData* app = WebKioskAppManager::Get()->GetAppByAccountId(
          *kiosk_app_id_.account_id);
      DCHECK(app);
      auto data = KioskAppManagerBase::App(*app);
      data.url = app->install_url();
      return data;
    }
  }
}

bool KioskLaunchController::IsNetworkRequired() {
  return network_required_;
}

void KioskLaunchController::CleanUp() {
  network_wait_timer_.Stop();
  splash_wait_timer_.Stop();

  kiosk_profile_loader_.reset();
  // Can be null in tests.
  if (host_)
    host_->Finalize(base::OnceClosure());
  // Make sure that any kiosk launch errors get written to disk before we kill
  // the browser.
  g_browser_process->local_state()->CommitPendingWrite();
}

void KioskLaunchController::OnTimerFire() {
  if (app_state_ == AppState::LAUNCHED) {
    CloseSplashScreen();
  } else if (app_state_ == AppState::INSTALLED) {
    LaunchApp();
  } else {
    launch_on_install_ = true;
  }
}

void KioskLaunchController::CloseSplashScreen() {
  CleanUp();
}

void KioskLaunchController::OnAppInstalling() {
  SYSLOG(INFO) << "Kiosk app started installing.";
  app_state_ = AppState::INSTALLING;
  if (!splash_screen_view_)
    return;
  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          APP_LAUNCH_STATE_INSTALLING_APPLICATION);

  splash_screen_view_->Show();
}

void KioskLaunchController::OnAppPrepared() {
  SYSLOG(INFO) << "Kiosk app is ready to launch.";
  app_state_ = AppState::INSTALLED;

  if (!splash_screen_view_)
    return;

  if (network_ui_state_ != NetworkUIState::NOT_SHOWING)
    return;

  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          APP_LAUNCH_STATE_WAITING_APP_WINDOW);
  splash_screen_view_->Show();
  if (launch_on_install_ || g_skip_splash_wait_for_testing)
    LaunchApp();
}

void KioskLaunchController::InitializeNetwork() {
  if (!splash_screen_view_)
    return;

  network_wait_timer_.Start(FROM_HERE, g_network_wait_time, this,
                            &KioskLaunchController::OnNetworkWaitTimedOut);

  // When we are asked to initialize network, we should remember that this app
  // requires network.
  network_required_ = true;

  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::APP_LAUNCH_STATE_PREPARING_NETWORK);

  app_state_ = AppState::INIT_NETWORK;

  if (splash_screen_view_->IsNetworkReady())
    OnNetworkStateChanged(true);
}

void KioskLaunchController::OnNetworkWaitTimedOut() {
  DCHECK_EQ(network_ui_state_, NetworkUIState::NOT_SHOWING);

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
  return network_ui_state_ == NetworkUIState::SHOWING;
}

bool KioskLaunchController::ShouldSkipAppInstallation() const {
  return false;
}

void KioskLaunchController::OnLaunchFailed(KioskAppLaunchError::Error error) {
  DCHECK_NE(KioskAppLaunchError::NONE, error);
  SYSLOG(ERROR) << "Kiosk launch failed, error=" << error;

  if (kiosk_app_id_.type == KioskAppType::WEB_APP) {
    HandleWebAppInstallFailed();
    return;
  }

  // Reboot on the recoverable cryptohome errors.
  if (error == KioskAppLaunchError::CRYPTOHOMED_NOT_RUNNING ||
      error == KioskAppLaunchError::ALREADY_MOUNTED) {
    // Do not save the error because saved errors would stop app from launching
    // on the next run.
    chrome::AttemptRelaunch();
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
  app_state_ = AppState::INSTALLED;

  SYSLOG(WARNING) << "Failed to obtain app data, trying to launch anyway..";

  if (!splash_screen_view_)
    return;
  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          APP_LAUNCH_STATE_WAITING_APP_WINDOW_INSTALL_FAILED);
  splash_screen_view_->Show();
  if (launch_on_install_ || g_skip_splash_wait_for_testing)
    LaunchApp();
}

void KioskLaunchController::OnAppLaunched() {
  SYSLOG(INFO) << "Kiosk launch succeeded, wait for app window.";
  app_state_ = AppState::LAUNCHED;
  if (splash_screen_view_) {
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::APP_LAUNCH_STATE_WAITING_APP_WINDOW);
    splash_screen_view_->Show();
  }
  session_manager::SessionManager::Get()->SessionStarted();
}

void KioskLaunchController::OnAppWindowCreated() {
  SYSLOG(INFO) << "App window created, closing splash screen.";
  // If timer is running, do not remove splash screen for a few
  // more seconds to give the user ability to exit kiosk session.
  if (splash_wait_timer_.IsRunning())
    return;
  CloseSplashScreen();
}

void KioskLaunchController::OnAppDataUpdated() {
  // Invokes Show() to update the app title and icon.
  if (splash_screen_view_)
    splash_screen_view_->Show();
}

void KioskLaunchController::OnProfileLoadFailed(
    KioskAppLaunchError::Error error) {
  OnLaunchFailed(error);
}

void KioskLaunchController::OnOldEncryptionDetected(
    const UserContext& user_context) {
  if (kiosk_app_id_.type != KioskAppType::ARC_APP) {
    NOTREACHED();
    return;
  }
  host_->StartWizard(EncryptionMigrationScreenView::kScreenId);
  EncryptionMigrationScreen* migration_screen =
      static_cast<EncryptionMigrationScreen*>(
          host_->GetWizardController()->current_screen());
  DCHECK(migration_screen);
  migration_screen->SetUserContext(user_context);
  migration_screen->SetupInitialView();
}

void KioskLaunchController::OnOwnerSigninSuccess() {
  ShowNetworkConfigureUI();
  signin_screen_.reset();
}

bool KioskLaunchController::CanConfigureNetwork() {
  if (can_configure_network_callback)
    return can_configure_network_callback->Run();

  if (IsEnterpriseManaged()) {
    bool should_prompt;
    if (CrosSettings::Get()->GetBoolean(
            kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline,
            &should_prompt)) {
      return should_prompt;
    }
    // Default to true to allow network configuration if the policy is missing.
    return true;
  }

  return user_manager::UserManager::Get()->GetOwnerAccountId().is_valid();
}

bool KioskLaunchController::NeedOwnerAuthToConfigureNetwork() {
  if (need_owner_auth_to_configure_network_callback)
    return need_owner_auth_to_configure_network_callback->Run();

  return !IsEnterpriseManaged();
}

void KioskLaunchController::MaybeShowNetworkConfigureUI() {
  SYSLOG(INFO) << "Network configure UI was requested to be shown.";
  if (!splash_screen_view_)
    return;

  if (CanConfigureNetwork()) {
    if (NeedOwnerAuthToConfigureNetwork()) {
      if (!network_wait_timedout_)
        OnConfigureNetwork();
      else
        splash_screen_view_->ToggleNetworkConfig(true);
    } else {
      ShowNetworkConfigureUI();
    }
  } else {
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::APP_LAUNCH_STATE_NETWORK_WAIT_TIMEOUT);
  }
}

void KioskLaunchController::ShowNetworkConfigureUI() {
  if (!profile_) {
    SYSLOG(INFO) << "Postponing network dialog till profile is loaded.";
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::
            APP_LAUNCH_STATE_SHOWING_NETWORK_CONFIGURE_UI);
    return;
  }
  // We should stop timers since they may fire during network
  // configure UI.
  splash_wait_timer_.Stop();
  network_wait_timer_.Stop();
  launch_on_install_ = true;
  network_ui_state_ = NetworkUIState::SHOWING;
  splash_screen_view_->ShowNetworkConfigureUI();
}

void KioskLaunchController::CloseNetworkConfigureScreenIfOnline() {
  if (network_ui_state_ == NetworkUIState::SHOWING && network_wait_timedout_) {
    SYSLOG(INFO) << "We are back online, closing network configure screen.";
    splash_screen_view_->ToggleNetworkConfig(false);
    network_ui_state_ = NetworkUIState::NOT_SHOWING;
  }
}

void KioskLaunchController::OnNetworkConfigRequested() {
  network_ui_state_ = NetworkUIState::NEED_TO_SHOW;
  switch (app_state_) {
    case AppState::CREATING_PROFILE:
    case AppState::INIT_NETWORK:
    case AppState::INSTALLED:
      MaybeShowNetworkConfigureUI();
      break;
    case AppState::INSTALLING:
      // When requesting to show network configure UI, we should cancel current
      // installation and restart it as soon as the network is configured.
      app_state_ = AppState::INIT_NETWORK;
      app_launcher_->RestartLauncher();
      MaybeShowNetworkConfigureUI();
      break;
    case AppState::LAUNCHED:
      // We do nothing since the splash screen is soon to be destroyed.
      break;
  }
}

void KioskLaunchController::OnNetworkConfigFinished() {
  network_ui_state_ = NetworkUIState::NOT_SHOWING;
  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::APP_LAUNCH_STATE_PREPARING_PROFILE);
  app_state_ = AppState::INIT_NETWORK;
  app_launcher_->RestartLauncher();
}

void KioskLaunchController::OnNetworkStateChanged(bool online) {
  if (app_state_ == AppState::INIT_NETWORK && online) {
    // If the network timed out, we should exit network config dialog as soon as
    // we are back online.
    if (network_ui_state_ == NetworkUIState::NOT_SHOWING ||
        network_wait_timedout_) {
      network_wait_timer_.Stop();
      CloseNetworkConfigureScreenIfOnline();
      app_launcher_->ContinueWithNetworkReady();
    }
  }

  if (app_state_ == AppState::INSTALLING && network_required_ && !online) {
    SYSLOG(WARNING)
        << "Connection lost during installation, restarting launcher.";
    OnNetworkWaitTimedOut();
  }
}

void KioskLaunchController::LaunchApp() {
  if (g_block_app_launch_for_testing)
    return;

  DCHECK(app_state_ == AppState::INSTALLED);
  // We need to change the session state so we are able to create browser
  // windows.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  splash_wait_timer_.Stop();
  app_launcher_->LaunchApp();
}

// static
std::unique_ptr<base::AutoReset<bool>>
KioskLaunchController::DisableWaitTimerAndLoginOperationsForTesting() {
  return std::make_unique<base::AutoReset<bool>>(
      &g_disable_wait_timer_and_login_operations, true);
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

// static
std::unique_ptr<KioskLaunchController> KioskLaunchController::CreateForTesting(
    AppLaunchSplashScreenView* view,
    std::unique_ptr<KioskAppLauncher> app_launcher) {
  std::unique_ptr<KioskLaunchController> controller(
      new KioskLaunchController());
  controller->splash_screen_view_ = view;
  controller->app_launcher_ = std::move(app_launcher);
  return controller;
}

}  // namespace chromeos
