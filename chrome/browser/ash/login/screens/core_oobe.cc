// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/core_oobe.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace ash {

CoreOobe::CoreOobe(const std::string& display_type,
                   base::WeakPtr<CoreOobeView> view)
    : view_(view) {
  is_oobe_display_ = display_type == OobeUI::kOobeDisplay;

  OobeConfiguration::Get()->AddAndFireObserver(this);
  ChromeKeyboardControllerClient::Get()->AddObserver(this);

  OnKeyboardVisibilityChanged(
      ChromeKeyboardControllerClient::Get()->is_keyboard_visible());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  version_info_updater_.StartUpdate(true);
#else
  version_info_updater_.StartUpdate(false);
#endif

  OnTabletModeChanged(display::Screen::GetScreen()->InTabletMode());
  UpdateClientAreaSize(
      display::Screen::GetScreen()->GetPrimaryDisplay().size());

  // Don't show version label on the stable and beta channels by default.
  version_info::Channel channel = chrome::GetChannel();
  if (channel != version_info::Channel::STABLE &&
      channel != version_info::Channel::BETA) {
    if (view_) {
      view_->ToggleSystemInfo();
    }
  }

  if (ash::system::InputDeviceSettings::Get()
          ->ForceKeyboardDrivenUINavigation()) {
    if (view_) {
      view_->EnableKeyboardFlow();
    }
  }
}

CoreOobe::~CoreOobe() {
  OobeConfiguration::Get()->RemoveObserver(this);

  if (ChromeKeyboardControllerClient::Get()) {
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  }
}

void CoreOobe::ShowScreenWithData(const OobeScreenId& screen,
                                  std::optional<base::Value::Dict> data) {
  const bool is_priority_screen =
      PriorityScreenChecker::IsPriorityScreen(screen);

  switch (ui_init_state_) {
    case CoreOobeView::UiState::kUninitialized:
    case CoreOobeView::UiState::kCoreHandlerInitialized:
      // In this early state, we defer all show screen calls.
      pending_calls_.show_screen_with_data =
          base::BindOnce(&CoreOobe::ShowScreenWithData, base::Unretained(this),
                         screen, std::move(data));
      return;
    case CoreOobeView::UiState::kPriorityScreensLoaded:
      // Priority screens can be shown at this point. All others are deferred.
      if (!is_priority_screen || !features::IsOobeLazyLoadingEnabled()) {
        pending_calls_.show_screen_with_data =
            base::BindOnce(&CoreOobe::ShowScreenWithData,
                           base::Unretained(this), screen, std::move(data));
        return;
      }
      break;
    case CoreOobeView::UiState::kFullyInitialized:
      // All screens can be shown at this point.
      break;
  }
  // Clear any pending calls that could exist. (An edge case when a priority
  // screen is shown when a regular screen has been deferred. No known instances
  // in the codebase) Mostly a sanity check.
  pending_calls_.show_screen_with_data.Reset();

  // Since CoreOobeHandler is always created before us, this call can never be
  // silently dropped under normal circumstances.
  if (view_) {
    view_->ShowScreenWithData(screen, std::move(data));
  }
}

void CoreOobe::ReloadContent() {
  // Defer until fully initialized.
  if (ui_init_state_ != CoreOobeView::UiState::kFullyInitialized) {
    pending_calls_.reload_content =
        base::BindOnce(&CoreOobe::ReloadContent, base::Unretained(this));
    return;
  }

  if (view_) {
    view_->ReloadContent();
  }
}

void CoreOobe::ForwardCancel() {
  // Defer until fully initialized.
  if (ui_init_state_ != CoreOobeView::UiState::kFullyInitialized) {
    pending_calls_.forward_cancel =
        base::BindOnce(&CoreOobe::ForwardCancel, base::Unretained(this));
    return;
  }

  if (view_) {
    view_->ForwardCancel();
  }
}

void CoreOobe::UpdateClientAreaSize(const gfx::Size& size) {
  if (!view_) {
    return;
  }
  view_->SetShelfHeight(ShelfConfig::Get()->shelf_size());

  const gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  const bool is_horizontal = display_size.width() > display_size.height();
  view_->SetOrientation(is_horizontal);

  const gfx::Size dialog_size = CalculateOobeDialogSize(
      size, ShelfConfig::Get()->shelf_size(), is_horizontal);
  view_->SetDialogSize(dialog_size.width(), dialog_size.height());
}

void CoreOobe::TriggerDown() {
  if (view_) {
    view_->TriggerDown();
  }
}

void CoreOobe::ToggleSystemInfo() {
  if (view_) {
    view_->ToggleSystemInfo();
  }
}

void CoreOobe::LaunchHelpApp(int help_topic_id) {
  if (!help_app_.get()) {
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  }
  help_app_->ShowHelpTopic(
      static_cast<HelpAppLauncher::HelpTopic>(help_topic_id));
}

void CoreOobe::OnOSVersionLabelTextUpdated(const std::string& os_version_text) {
  if (view_) {
    view_->SetOsVersionLabelText(os_version_text);
  }
}

void CoreOobe::OnDeviceInfoUpdated(const std::string& bluetooth_name) {
  if (view_) {
    view_->SetBluetoothDeviceInfo(bluetooth_name);
  }
}

void CoreOobe::OnKeyboardVisibilityChanged(bool shown) {
  if (view_) {
    view_->SetVirtualKeyboardShown(shown);
  }
}

void CoreOobe::OnDisplayTabletStateChanged(display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      OnTabletModeChanged(/*tablet_mode_enabled=*/true);
      break;
    case display::TabletState::kInClamshellMode:
      OnTabletModeChanged(/*tablet_mode_enabled=*/false);
      break;
  }
}

void CoreOobe::OnOobeConfigurationChanged() {
  // Defer until fully initialized.
  if (ui_init_state_ != CoreOobeView::UiState::kFullyInitialized) {
    pending_calls_.on_oobe_configuration_changed = base::BindOnce(
        &CoreOobe::OnOobeConfigurationChanged, base::Unretained(this));
    return;
  }

  if (view_) {
    view_->UpdateOobeConfiguration();
  }
}

void CoreOobe::UpdateUiInitState(CoreOobeView::UiState state) {
  switch (state) {
    case CoreOobeView::UiState::kUninitialized:
      NOTREACHED_IN_MIGRATION();
      break;
    case CoreOobeView::UiState::kCoreHandlerInitialized:
      // JavaScript is now allowed in the handler.
      CHECK(ui_init_state_ == CoreOobeView::UiState::kUninitialized);
      ui_init_state_ = CoreOobeView::UiState::kCoreHandlerInitialized;
      break;
    case CoreOobeView::UiState::kPriorityScreensLoaded:
      CHECK(ui_init_state_ == CoreOobeView::UiState::kCoreHandlerInitialized);
      ui_init_state_ = CoreOobeView::UiState::kPriorityScreensLoaded;
      if (features::IsOobeLazyLoadingEnabled()) {
        MaybeShowPriorityScreen();
      }
      break;
    case CoreOobeView::UiState::kFullyInitialized:
      // OOBE is fully loaded.
      CHECK(ui_init_state_ == CoreOobeView::UiState::kPriorityScreensLoaded);
      ui_init_state_ = CoreOobeView::UiState::kFullyInitialized;
      // Execute deferred JavaScript in the CoreHandler first.
      ExecutePendingCalls();
      // Call AllowJavascript() on all handlers.
      if (LoginDisplayHost::default_host() &&
          LoginDisplayHost::default_host()->GetOobeUI()) {
        LoginDisplayHost::default_host()->GetOobeUI()->InitializeHandlers();
      } else {
        LOG(ERROR)
            << "OOBE: Not initializing handlers because LoginDisplayHost or "
               "OobeUI does not exist!";
      }
      break;
  }
}

void CoreOobe::ExecutePendingCalls() {
  CHECK(ui_init_state_ == CoreOobeView::UiState::kFullyInitialized);
  if (pending_calls_.on_tablet_mode_changed) {
    std::move(pending_calls_.on_tablet_mode_changed).Run();
  }
  if (pending_calls_.on_oobe_configuration_changed) {
    std::move(pending_calls_.on_oobe_configuration_changed).Run();
  }
  if (pending_calls_.show_screen_with_data) {
    std::move(pending_calls_.show_screen_with_data).Run();
  }
  if (pending_calls_.reload_content) {
    std::move(pending_calls_.reload_content).Run();
  }
  if (pending_calls_.forward_cancel) {
    std::move(pending_calls_.forward_cancel).Run();
  }
}

void CoreOobe::MaybeShowPriorityScreen() {
  CHECK(features::IsOobeLazyLoadingEnabled());
  CHECK(ui_init_state_ == CoreOobeView::UiState::kPriorityScreensLoaded);
  // Run any pending show screen call. If the screen is not supported for
  // prioritization, ShowScreenWithData will defer it and it will be shown
  // once the UI fully initializes.
  if (pending_calls_.show_screen_with_data) {
    std::move(pending_calls_.show_screen_with_data).Run();
  }
}

void CoreOobe::OnTabletModeChanged(bool tablet_mode_enabled) {
  // Defer until fully initialized.
  if (ui_init_state_ != CoreOobeView::UiState::kFullyInitialized) {
    pending_calls_.on_tablet_mode_changed =
        base::BindOnce(&CoreOobe::OnTabletModeChanged, base::Unretained(this),
                       tablet_mode_enabled);
    return;
  }

  if (view_) {
    view_->SetTabletModeState(tablet_mode_enabled);
  }
}

CoreOobe::PendingFrontendCalls::PendingFrontendCalls() = default;
CoreOobe::PendingFrontendCalls::~PendingFrontendCalls() = default;

}  // namespace ash
