// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/error_screen.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"
#include "chrome/browser/ash/login/screens/connectivity_diagnostics_dialog.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/captive_portal_window_proxy.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_display_host_mojo.h"
#include "chrome/browser/ui/ash/login/login_web_dialog.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

namespace {

// TODO(https://crbug.com/1241511): Remove this global.
bool g_offline_login_allowed_ = false;

// Additional flag applied on top of g_offline_login_allowed_ that can block
// visibility of offline login link when a focused pod user has offline login
// timer (set by policy) expired. If that happens flag is flipped to false.
// In all other cases it should be set to a default value of true.
// Even if a user gets to the (hidden) flow, the offline login may be blocked
// by checking the policy value there.
// TODO(https://crbug.com/1241511): Remove this global.
bool g_offline_login_per_user_allowed_ = true;

// Returns the current running kiosk app profile in a kiosk session. Otherwise,
// returns nullptr.
Profile* GetAppProfile() {
  return IsRunningInForcedAppMode() ? ProfileManager::GetActiveUserProfile()
                                    : nullptr;
}

}  // namespace

constexpr const char kUserActionConfigureCertsButtonClicked[] =
    "configure-certs";
constexpr const char kUserActionDiagnoseButtonClicked[] = "diagnose";
constexpr const char kUserActionLaunchOobeGuestSessionClicked[] =
    "launch-oobe-guest";
constexpr const char kUserActionRebootButtonClicked[] = "reboot";
constexpr const char kUserActionShowCaptivePortalClicked[] =
    "show-captive-portal";
constexpr const char kUserActionOpenInternetDialog[] = "open-internet-dialog";
constexpr const char kUserActionNetworkConnected[] = "network-connected";
constexpr const char kUserActionReloadGaia[] = "reload-gaia";
constexpr const char kUserActionCancelReset[] = "cancel-reset";
constexpr const char kUserActionCancel[] = "cancel";
constexpr const char kUserActionContinueAppLaunch[] = "continue-app-launch";
constexpr const char kUserActionOfflineLogin[] = "offline-login";

ErrorScreen::ErrorScreen(base::WeakPtr<ErrorScreenView> view)
    : BaseScreen(ErrorScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)) {
  network_state_informer_ = new NetworkStateInformer();
  network_state_informer_->Init();
  NetworkHandler::Get()->network_connection_handler()->AddObserver(this);
}

ErrorScreen::~ErrorScreen() {
  NetworkHandler::Get()->network_connection_handler()->RemoveObserver(this);
}

void ErrorScreen::AllowGuestSignin(bool allowed) {
  if (view_) {
    view_->SetGuestSigninAllowed(allowed);
  }
}

void ErrorScreen::DisallowOfflineLogin() {
  ShowOfflineLoginOption(false);
}

void ErrorScreen::ShowOfflineLoginOption(bool show) {
  if (view_) {
    view_->SetOfflineSigninAllowed(show);
  }
}

void ErrorScreen::OnOfflineLoginClicked() {
  // Reset hide callback as we advance to OfflineLoginScreen. Exit from this
  // screen is handled by WizardController.
  on_hide_callback_ = base::OnceClosure();
  Hide();
  LoginDisplayHost::default_host()->StartWizard(OfflineLoginView::kScreenId);
}

// static
void ErrorScreen::AllowOfflineLogin(bool allowed) {
  g_offline_login_allowed_ = allowed;
}

// static
void ErrorScreen::AllowOfflineLoginPerUser(bool allowed) {
  g_offline_login_per_user_allowed_ = allowed;
}

void ErrorScreen::FixCaptivePortal() {
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name =
      NetworkStateInformer::GetNetworkName(network_path);
  const auto state = network_state_informer_->state();
  if (network_name.empty() || state != NetworkStateInformer::CAPTIVE_PORTAL) {
    LOG(ERROR) << __func__ << " without network in a portalled state.";
    return;
  }
  MaybeInitCaptivePortalWindowProxy(
      LoginDisplayHost::default_host()->GetOobeWebContents());
  captive_portal_window_proxy_->ShowIfRedirected(network_name);
}

NetworkError::UIState ErrorScreen::GetUIState() const {
  return ui_state_;
}

NetworkError::ErrorState ErrorScreen::GetErrorState() const {
  return error_state_;
}

OobeScreenId ErrorScreen::GetParentScreen() const {
  return parent_screen_;
}

void ErrorScreen::HideCaptivePortal() {
  if (captive_portal_window_proxy_.get()) {
    captive_portal_window_proxy_->Close();
  }
}

void ErrorScreen::SetUIState(NetworkError::UIState ui_state) {
  LOG(WARNING) << __func__ << " to " << ui_state;
  ui_state_ = ui_state;
  if (view_) {
    view_->SetUIState(ui_state);
  }
}

void ErrorScreen::SetErrorState(NetworkError::ErrorState error_state,
                                const std::string& network) {
  LOG(WARNING) << __func__ << " to " << error_state;
  error_state_ = error_state;
  if (view_) {
    view_->SetErrorStateCode(error_state);
    view_->SetErrorStateNetwork(network);
  }
}

void ErrorScreen::SetParentScreen(OobeScreenId parent_screen) {
  parent_screen_ = parent_screen;
  // Not really used on JS side yet so no need to propagate to screen context.
}

void ErrorScreen::SetHideCallback(base::OnceClosure on_hide) {
  on_hide_callback_ = std::move(on_hide);
}

void ErrorScreen::ShowCaptivePortal() {
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name =
      NetworkStateInformer::GetNetworkName(network_path);
  const auto state = network_state_informer_->state();
  if (network_name.empty() || state != NetworkStateInformer::CAPTIVE_PORTAL) {
    LOG(ERROR) << __func__ << " without network in a portalled state.";
    return;
  }

  // This call is an explicit user action
  // i.e. clicking on link so force dialog show.
  FixCaptivePortal();
  captive_portal_window_proxy_->Show(network_name);
}

void ErrorScreen::ShowConnectingIndicator(bool show) {
  if (view_) {
    view_->SetShowConnectingIndicator(show);
  }
}

void ErrorScreen::SetIsPersistentError(bool is_persistent) {
  is_persistent_ = is_persistent;
}

base::CallbackListSubscription ErrorScreen::RegisterConnectRequestCallback(
    base::RepeatingClosure callback) {
  return connect_request_callbacks_.Add(std::move(callback));
}

void ErrorScreen::MaybeInitCaptivePortalWindowProxy(
    content::WebContents* web_contents) {
  if (!captive_portal_window_proxy_.get()) {
    captive_portal_window_proxy_ =
        std::make_unique<CaptivePortalWindowProxy>(web_contents);
  }
}

void ErrorScreen::ShowNetworkErrorMessage(NetworkStateInformer::State state,
                                          NetworkError::ErrorReason reason) {
  LOG(WARNING) << __func__ << " state = " << state << " reason = " << reason;
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name =
      NetworkStateInformer::GetNetworkName(network_path);

  const bool is_behind_captive_portal =
      state == NetworkStateInformer::CAPTIVE_PORTAL;
  const bool is_proxy_error = NetworkStateInformer::IsProxyError(state, reason);
  const bool is_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);

  if (!is_behind_captive_portal) {
    HideCaptivePortal();
  }

  if (is_proxy_error) {
    SetErrorState(NetworkError::ERROR_STATE_PROXY, std::string());
  } else if (is_behind_captive_portal) {
    if (GetErrorState() != NetworkError::ERROR_STATE_PORTAL) {
      LoginDisplayHost::default_host()->HandleDisplayCaptivePortal();
    }
    SetErrorState(NetworkError::ERROR_STATE_PORTAL, network_name);
  } else if (is_loading_timeout) {
    SetErrorState(NetworkError::ERROR_STATE_LOADING_TIMEOUT, network_name);
  } else {
    SetErrorState(NetworkError::ERROR_STATE_OFFLINE, std::string());
  }

  const bool guest_signin_allowed =
      user_manager::UserManager::Get()->IsGuestSessionAllowed();
  AllowGuestSignin(guest_signin_allowed);
  ShowOfflineLoginOption(
      g_offline_login_allowed_ && g_offline_login_per_user_allowed_ &&
      GetErrorState() != NetworkError::ERROR_STATE_LOADING_TIMEOUT);

  // No need to show the screen again if it is already shown.
  if (is_hidden()) {
    SetUIState(NetworkError::UI_STATE_SIGNIN);
    Show(/*wizard_context=*/nullptr);
  }
}

void ErrorScreen::ShowImpl() {
  if (!on_hide_callback_) {
    SetHideCallback(base::BindOnce(&ErrorScreen::DefaultHideCallback,
                                   weak_factory_.GetWeakPtr()));
  }
  if (!view_) {
    return;
  }

  const bool is_closeable = LoginDisplayHost::default_host() &&
                            LoginDisplayHost::default_host()->HasUserPods() &&
                            !is_persistent_;
  view_->ShowScreenWithParam(is_closeable);
  LOG(WARNING) << "Network error screen message is shown";
}

void ErrorScreen::HideImpl() {
  if (!view_ || is_hidden()) {
    return;
  }

  LOG(WARNING) << "Network error screen message is hidden";
  if (on_hide_callback_) {
    std::move(on_hide_callback_).Run();
    on_hide_callback_ = base::OnceClosure();
  }
}

void ErrorScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionShowCaptivePortalClicked) {
    ShowCaptivePortal();
  } else if (action_id == kUserActionOpenInternetDialog) {
    // Empty string opens the internet detail dialog for the default network.
    InternetDetailDialog::ShowDialog("");
  } else if (action_id == kUserActionConfigureCertsButtonClicked) {
    OnConfigureCerts();
  } else if (action_id == kUserActionDiagnoseButtonClicked) {
    OnDiagnoseButtonClicked();
  } else if (action_id == kUserActionLaunchOobeGuestSessionClicked) {
    OnLaunchOobeGuestSession();
  } else if (action_id == kUserActionRebootButtonClicked) {
    OnRebootButtonClicked();
  } else if (action_id == kUserActionCancel) {
    OnCancelButtonClicked();
  } else if (action_id == kUserActionReloadGaia) {
    OnReloadGaiaClicked();
  } else if (action_id == kUserActionNetworkConnected) {
    // JS network implementation might notify that the network was connected
    // faster than the corresponding C++ code. Let the screen on which error is
    // shown handle `ErrorScreen::Hide`
    if (network_state_informer_->state() == NetworkStateInformer::ONLINE) {
      Hide();
    }
  } else if (action_id == kUserActionCancelReset) {
    Hide();
  } else if (action_id == kUserActionOfflineLogin) {
    OnOfflineLoginClicked();
  } else if (action_id == kUserActionContinueAppLaunch) {
    OnContinueAppLaunchButtonClicked();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void ErrorScreen::DefaultHideCallback() {
  if (parent_screen_ != OOBE_SCREEN_UNKNOWN && view_) {
    view_->ShowOobeScreen(parent_screen_);
  }

  // TODO(antrim): Due to potential race with GAIA reload and hiding network
  // error UI we can't just reset parent screen to SCREEN_UNKNOWN here.
}

void ErrorScreen::OnConfigureCerts() {
  gfx::NativeWindow native_window =
      LoginDisplayHost::default_host()->GetNativeWindow();
  LoginWebDialog* dialog = new LoginWebDialog(
      GetAppProfile(), native_window,
      l10n_util::GetStringUTF16(IDS_CERTIFICATE_MANAGER_TITLE),
      GURL(chrome::kChromeUICertificateManagerDialogURL));
  // The width matches the Settings UI width.
  dialog->set_dialog_size(gfx::Size{640, 480});
  dialog->Show();
}

void ErrorScreen::OnDiagnoseButtonClicked() {
  gfx::NativeWindow native_window =
      LoginDisplayHost::default_host()->GetNativeWindow();
  ConnectivityDiagnosticsDialog::ShowDialog(native_window);
}

void ErrorScreen::OnLaunchOobeGuestSession() {
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::BindOnce(&ErrorScreen::StartGuestSessionAfterOwnershipCheck,
                     weak_factory_.GetWeakPtr()));
}

void ErrorScreen::OnRebootButtonClicked() {
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_FOR_USER, "login error screen");
}

void ErrorScreen::OnCancelButtonClicked() {
  DCHECK(LoginDisplayHost::default_host()->HasUserPods());
  LoginDisplayHost::default_host()->HideOobeDialog();
  Hide();
}

void ErrorScreen::OnReloadGaiaClicked() {
  DCHECK_EQ(parent_screen_, GaiaView::kScreenId.AsId());
  WizardController::default_controller()
      ->GetScreen<GaiaScreen>()
      ->ReloadGaiaAuthenticator();
}

void ErrorScreen::OnContinueAppLaunchButtonClicked() {
  DCHECK_EQ(parent_screen_, AppLaunchSplashScreenView::kScreenId.AsId());
  WizardController::default_controller()
      ->GetScreen<AppLaunchSplashScreen>()
      ->ContinueAppLaunch();
}

void ErrorScreen::LaunchHelpApp(int help_topic_id) {
  if (!help_app_.get()) {
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  }
  help_app_->ShowHelpTopic(
      static_cast<HelpAppLauncher::HelpTopic>(help_topic_id));
}

void ErrorScreen::ConnectToNetworkRequested(const std::string& service_path) {
  connect_request_callbacks_.Notify();
}

void ErrorScreen::StartGuestSessionAfterOwnershipCheck(
    DeviceSettingsService::OwnershipStatus ownership_status) {
  // Make sure to disallow guest login if it's explicitly disabled.
  CrosSettingsProvider::TrustedStatus trust_status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&ErrorScreen::StartGuestSessionAfterOwnershipCheck,
                         weak_factory_.GetWeakPtr(), ownership_status));
  switch (trust_status) {
    case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Wait for a callback.
      return;
    case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // Only allow guest sessions if there is no owner yet.
      if (ownership_status ==
          DeviceSettingsService::OwnershipStatus::kOwnershipNone) {
        break;
      }
      return;
    case CrosSettingsProvider::TRUSTED: {
      // Honor kAccountsPrefAllowGuest.
      bool allow_guest = false;
      CrosSettings::Get()->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
      if (allow_guest) {
        break;
      }
      return;
    }
  }

  // If EULA was not accepted yet, Show the Guest ToS screen.
  if (!StartupUtils::IsEulaAccepted()) {
    if (LoginDisplayHost::default_host()) {
      LoginDisplayHost::default_host()->ShowGuestTosScreen();
    } else {
      LOG(ERROR) << "Failed to show Guest ToS screen.";
    }
    return;
  }

  LoginDisplayHost::default_host()->GetExistingUserController()->Login(
      UserContext(user_manager::UserType::kGuest,
                  user_manager::GuestAccountId()),
      SigninSpecifics());
}

}  // namespace ash
