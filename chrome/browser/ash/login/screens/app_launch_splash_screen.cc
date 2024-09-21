// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"

#include <string>

#include "ash/public/cpp/login_screen.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr const char kUserActionConfigureNetwork[] = "configure-network";

std::string NameOrDefault(std::string_view name) {
  return name.empty() ? l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME)
                      : std::string(name);
}

gfx::ImageSkia IconOrDefault(gfx::ImageSkia icon) {
  return icon.isNull()
             ? *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                   IDR_PRODUCT_LOGO_128)
             : icon;
}

base::Value::Dict ConvertDataToDict(const AppLaunchSplashScreen::Data& data) {
  return base::Value::Dict()
      .Set("name", data.name)
      .Set("iconURL", webui::GetBitmapDataUrl(*data.icon.bitmap()))
      .Set("url", data.url.spec());
}

base::Value::Dict GetScreenData(const AppLaunchSplashScreen::Data& data) {
  return base::Value::Dict()
      .Set("shortcutEnabled",
           !KioskChromeAppManager::Get()->GetDisableBailoutShortcut())
      .Set("appInfo", ConvertDataToDict(data));
}

}  // namespace

AppLaunchSplashScreen::Data::Data(std::string_view name,
                                  gfx::ImageSkia icon,
                                  const GURL& url)
    : name(NameOrDefault(name)),
      icon(IconOrDefault(icon)),
      url(url.DeprecatedGetOriginAsURL()) {}
AppLaunchSplashScreen::Data::Data(Data&&) = default;
AppLaunchSplashScreen::Data& AppLaunchSplashScreen::Data::operator=(Data&&) =
    default;
AppLaunchSplashScreen::Data::~Data() = default;

AppLaunchSplashScreen::AppLaunchSplashScreen(
    base::WeakPtr<AppLaunchSplashScreenView> view,
    ErrorScreen* error_screen,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(AppLaunchSplashScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      error_screen_(error_screen),
      exit_callback_(exit_callback) {}

AppLaunchSplashScreen::~AppLaunchSplashScreen() = default;

void AppLaunchSplashScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  UpdateAppLaunchState(state_);
  base::Value::Dict screen_data = GetScreenData(app_data_);
  view_->Show(std::move(screen_data));

  if (toggle_network_config_on_show_.has_value()) {
    ToggleNetworkConfig(toggle_network_config_on_show_.value());
    toggle_network_config_on_show_.reset();
  }
}

void AppLaunchSplashScreen::HideImpl() {}

void AppLaunchSplashScreen::UpdateAppLaunchState(
    AppLaunchSplashScreenView::AppLaunchState state) {
  if (state == state_) {
    return;
  }
  state_ = state;

  if (view_) {
    view_->UpdateAppLaunchText(state_);
  }
}

void AppLaunchSplashScreen::ShowNetworkConfigureUI(
    NetworkStateInformer::State network_state,
    const std::string& network_name) {
  error_screen_->SetUIState(NetworkError::UI_STATE_KIOSK_MODE);
  error_screen_->SetIsPersistentError(true);
  error_screen_->AllowGuestSignin(false);
  error_screen_->DisallowOfflineLogin();
  switch (network_state) {
    case NetworkStateInformer::CAPTIVE_PORTAL: {
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                   network_name);
      error_screen_->FixCaptivePortal();

      break;
    }
    case NetworkStateInformer::PROXY_AUTH_REQUIRED: {
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                   network_name);
      break;
    }
    case NetworkStateInformer::ONLINE: {
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_KIOSK_ONLINE,
                                   network_name);
      break;
    }
    case NetworkStateInformer::OFFLINE:
    case NetworkStateInformer::CONNECTING:
    case NetworkStateInformer::UNKNOWN:
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                   network_name);
      break;
  }

  if (LoginDisplayHost::default_host()->GetOobeUI()->current_screen() !=
      ErrorScreenView::kScreenId) {
    error_screen_->SetParentScreen(AppLaunchSplashScreenView::kScreenId);
    error_screen_->Show(/*context=*/nullptr);
  }
}

void AppLaunchSplashScreen::ToggleNetworkConfig(bool visible) {
  if (is_hidden()) {
    toggle_network_config_on_show_ = visible;
    return;
  }
  view_->ToggleNetworkConfig(visible);
}

void AppLaunchSplashScreen::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void AppLaunchSplashScreen::SetAppData(Data data) {
  app_data_ = std::move(data);
  if (is_hidden()) {
    return;
  }

  base::Value::Dict screen_data = GetScreenData(data);
  view_->SetAppData(std::move(screen_data));
}

void AppLaunchSplashScreen::ShowErrorMessage(KioskAppLaunchError::Error error) {
  LoginScreen::Get()->ShowKioskAppError(
      KioskAppLaunchError::GetErrorMessage(error));
}

void AppLaunchSplashScreen::HandleConfigureNetwork() {
  if (delegate_) {
    delegate_->OnConfigureNetwork();
  } else {
    LOG(WARNING) << "No delegate set to handle network configuration.";
  }
}
void AppLaunchSplashScreen::ContinueAppLaunch() {
  if (!delegate_) {
    return;
  }

  delegate_->OnNetworkConfigFinished();

  // Reset ErrorScreen state to default. We don't update other parameters such
  // as SetUIState/SetErrorState as those should be updated by the next caller
  // of the ErrorScreen.
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  error_screen_->SetIsPersistentError(false);
  error_screen_->Hide();
}

void AppLaunchSplashScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionConfigureNetwork) {
    HandleConfigureNetwork();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
