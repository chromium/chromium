// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/error_screen.h"

#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/certificate_manager_dialog.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/auth/chrome_login_performer.h"
#include "chrome/browser/chromeos/login/chrome_restart_request.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "components/session_manager/core/session_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

namespace {

// Returns the current running kiosk app profile in a kiosk session. Otherwise,
// returns nullptr.
Profile* GetAppProfile() {
  return chrome::IsRunningInForcedAppMode()
             ? ProfileManager::GetActiveUserProfile()
             : nullptr;
}

}  // namespace

constexpr const char ErrorScreen::kUserActionConfigureCertsButtonClicked[] =
    "configure-certs";
constexpr const char ErrorScreen::kUserActionDiagnoseButtonClicked[] =
    "diagnose";
constexpr const char ErrorScreen::kUserActionLaunchOobeGuestSessionClicked[] =
    "launch-oobe-guest";
constexpr const char
    ErrorScreen::kUserActionLocalStateErrorPowerwashButtonClicked[] =
        "local-state-error-powerwash";
constexpr const char ErrorScreen::kUserActionRebootButtonClicked[] = "reboot";
constexpr const char ErrorScreen::kUserActionShowCaptivePortalClicked[] =
    "show-captive-portal";
constexpr const char ErrorScreen::kUserActionNetworkConnected[] =
    "network-connected";

ErrorScreen::ErrorScreen(ErrorScreenView* view)
    : BaseScreen(ErrorScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view) {
  network_state_informer_ = new NetworkStateInformer();
  network_state_informer_->Init();
  NetworkHandler::Get()->network_connection_handler()->AddObserver(this);
  if (view_)
    view_->Bind(this);
}

ErrorScreen::~ErrorScreen() {
  NetworkHandler::Get()->network_connection_handler()->RemoveObserver(this);
  if (view_)
    view_->Unbind();
}

void ErrorScreen::AllowGuestSignin(bool allowed) {
  if (view_)
    view_->SetGuestSigninAllowed(allowed);
}

void ErrorScreen::AllowOfflineLogin(bool allowed) {
  if (view_)
    view_->SetOfflineSigninAllowed(allowed);
}

void ErrorScreen::FixCaptivePortal() {
  MaybeInitCaptivePortalWindowProxy(
      LoginDisplayHost::default_host()->GetOobeWebContents());
  captive_portal_window_proxy_->ShowIfRedirected();
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
  if (captive_portal_window_proxy_.get())
    captive_portal_window_proxy_->Close();
}

void ErrorScreen::OnViewDestroyed(ErrorScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void ErrorScreen::SetUIState(NetworkError::UIState ui_state) {
  ui_state_ = ui_state;
  if (view_)
    view_->SetUIState(ui_state);
}

void ErrorScreen::SetErrorState(NetworkError::ErrorState error_state,
                                const std::string& network) {
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
  // This call is an explicit user action
  // i.e. clicking on link so force dialog show.
  FixCaptivePortal();
  captive_portal_window_proxy_->Show();
}

void ErrorScreen::ShowConnectingIndicator(bool show) {
  if (view_)
    view_->SetShowConnectingIndicator(show);
}

void ErrorScreen::SetIsPersistentError(bool is_persistent) {
  if (view_)
    view_->SetIsPersistentError(is_persistent);
}

ErrorScreen::ConnectRequestCallbackSubscription
ErrorScreen::RegisterConnectRequestCallback(const base::Closure& callback) {
  return connect_request_callbacks_.Add(callback);
}

void ErrorScreen::MaybeInitCaptivePortalWindowProxy(
    content::WebContents* web_contents) {
  if (!captive_portal_window_proxy_.get()) {
    captive_portal_window_proxy_ = std::make_unique<CaptivePortalWindowProxy>(
        network_state_informer_.get(), web_contents);
  }
}

void ErrorScreen::DoShow() {
  LOG(WARNING) << "Network error screen message is shown";
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
  network_portal_detector::GetInstance()->SetStrategy(
      PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN);
}

void ErrorScreen::DoHide() {
  LOG(WARNING) << "Network error screen message is hidden";
  if (on_hide_callback_) {
    std::move(on_hide_callback_).Run();
    on_hide_callback_ = base::OnceClosure();
  }
  network_portal_detector::GetInstance()->SetStrategy(
      PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN);
}

void ErrorScreen::SetupNetworkErrorMessage(NetworkStateInformer::State state,
                                           NetworkError::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name =
      NetworkStateInformer::GetNetworkName(network_path);

  const bool is_behind_captive_portal =
      NetworkStateInformer::IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = NetworkStateInformer::IsProxyError(state, reason);
  const bool is_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);

  if (!is_behind_captive_portal)
    HideCaptivePortal();

  if (is_proxy_error) {
    SetErrorState(NetworkError::ERROR_STATE_PROXY, std::string());
  } else if (is_behind_captive_portal) {
    if (GetErrorState() != NetworkError::ERROR_STATE_PORTAL) {
      LoginDisplayHost::default_host()->HandleDisplayCaptivePortal();
    }
    SetErrorState(NetworkError::ERROR_STATE_PORTAL, network_name);
  } else if (is_loading_timeout) {
    SetErrorState(NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT, network_name);
  } else {
    SetErrorState(NetworkError::ERROR_STATE_OFFLINE, std::string());
  }

  const bool guest_signin_allowed =
      user_manager::UserManager::Get()->IsGuestSessionAllowed();
  const bool offline_login_allowed =
      GetErrorState() != NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT;
  AllowGuestSignin(guest_signin_allowed);
  AllowOfflineLogin(offline_login_allowed);
}

void ErrorScreen::ShowImpl() {
  if (!on_hide_callback_) {
    SetHideCallback(base::BindOnce(&ErrorScreen::DefaultHideCallback,
                                   weak_factory_.GetWeakPtr()));
  }
  if (view_)
    view_->Show();
}

void ErrorScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void ErrorScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionShowCaptivePortalClicked)
    ShowCaptivePortal();
  else if (action_id == kUserActionConfigureCertsButtonClicked)
    OnConfigureCerts();
  else if (action_id == kUserActionDiagnoseButtonClicked)
    OnDiagnoseButtonClicked();
  else if (action_id == kUserActionLaunchOobeGuestSessionClicked)
    OnLaunchOobeGuestSession();
  else if (action_id == kUserActionLocalStateErrorPowerwashButtonClicked)
    OnLocalStateErrorPowerwashButtonClicked();
  else if (action_id == kUserActionRebootButtonClicked)
    OnRebootButtonClicked();
  else if (action_id == kUserActionNetworkConnected)
    Hide();
  else
    BaseScreen::OnUserAction(action_id);
}

void ErrorScreen::OnAuthFailure(const AuthFailure& error) {
  // The only condition leading here is guest mount failure, which should not
  // happen in practice. For now, just log an error so this situation is visible
  // in logs if it ever occurs.
  NOTREACHED() << "Guest login failed.";
  guest_login_performer_.reset();
}

void ErrorScreen::OnAuthSuccess(const UserContext& user_context) {
  LOG(FATAL);
}

void ErrorScreen::OnOffTheRecordAuthSuccess() {
  // Restart Chrome to enter the guest session.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line(browser_command_line.GetProgram());
  GetOffTheRecordCommandLine(GURL(), StartupUtils::IsOobeCompleted(),
                             browser_command_line, &command_line);
  RestartChrome(command_line);
}

void ErrorScreen::OnPasswordChangeDetected(const UserContext& user_context) {
  LOG(FATAL);
}

void ErrorScreen::AllowlistCheckFailed(const std::string& email) {
  LOG(FATAL);
}

void ErrorScreen::PolicyLoadFailed() {
  LOG(FATAL);
}

void ErrorScreen::SetAuthFlowOffline(bool offline) {
  LOG(FATAL);
}

void ErrorScreen::DefaultHideCallback() {
  if (parent_screen_ != OobeScreen::SCREEN_UNKNOWN && view_)
    view_->ShowOobeScreen(parent_screen_);

  // TODO(antrim): Due to potential race with GAIA reload and hiding network
  // error UI we can't just reset parent screen to SCREEN_UNKNOWN here.
}

void ErrorScreen::OnConfigureCerts() {
  gfx::NativeWindow native_window =
      LoginDisplayHost::default_host()->GetNativeWindow();
  CertificateManagerDialog* dialog =
      new CertificateManagerDialog(GetAppProfile(), NULL, native_window);
  dialog->Show();
}

void ErrorScreen::OnDiagnoseButtonClicked() {
  Profile* profile = GetAppProfile();
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  std::string extension_id = extension_service->component_loader()->Add(
      IDR_CONNECTIVITY_DIAGNOSTICS_MANIFEST,
      base::FilePath(extension_misc::kConnectivityDiagnosticsPath));

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithParams(apps::AppLaunchParams(
          extension_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW,
          apps::mojom::AppLaunchSource::kSourceChromeInternal));
  KioskAppManager::Get()->InitSession(profile, extension_id);

  LoginDisplayHost::default_host()->Finalize(base::BindOnce(
      [] { session_manager::SessionManager::Get()->SessionStarted(); }));
}

void ErrorScreen::OnLaunchOobeGuestSession() {
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&ErrorScreen::StartGuestSessionAfterOwnershipCheck,
                 weak_factory_.GetWeakPtr()));
}

void ErrorScreen::OnLocalStateErrorPowerwashButtonClicked() {
  SessionManagerClient::Get()->StartDeviceWipe();
}

void ErrorScreen::OnRebootButtonClicked() {
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_FOR_USER, "login error screen");
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
      if (ownership_status == DeviceSettingsService::OWNERSHIP_NONE)
        break;
      return;
    case CrosSettingsProvider::TRUSTED: {
      // Honor kAccountsPrefAllowGuest.
      bool allow_guest = false;
      CrosSettings::Get()->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
      if (allow_guest)
        break;
      return;
    }
  }

  if (guest_login_performer_)
    return;

  guest_login_performer_.reset(new ChromeLoginPerformer(this));
  guest_login_performer_->LoginOffTheRecord();
}

}  // namespace chromeos
