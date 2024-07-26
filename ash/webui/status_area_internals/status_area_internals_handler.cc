// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/status_area_internals/status_area_internals_handler.h"

#include <memory>
#include <utility>

#include "ash/annotator/annotator_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/fake_power_status.h"
#include "ash/system/model/fake_system_tray_model.h"
#include "ash/system/model/scoped_fake_power_status.h"
#include "ash/system/model/scoped_fake_system_tray_model.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "components/prefs/pref_service.h"

namespace ash {

StatusAreaInternalsHandler::StatusAreaInternalsHandler(
    mojo::PendingReceiver<mojom::status_area_internals::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {
  // When the web UI is in used, we will use a fake system tray model and a fake
  // power status  to mock the data shown in the system tray, then  switch back
  // to use the real model when the web UI is destructed using the scoped
  // setter.
  scoped_fake_model_ = std::make_unique<ScopedFakeSystemTrayModel>();
  scoped_fake_power_status_ = std::make_unique<ScopedFakePowerStatus>();
}

StatusAreaInternalsHandler::~StatusAreaInternalsHandler() = default;

void StatusAreaInternalsHandler::ToggleImeTray(bool visible) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(/*show=*/visible);
}

void StatusAreaInternalsHandler::TogglePaletteTray(bool visible) {
  if (visible) {
    stylus_utils::SetHasStylusInputForTesting();
  } else {
    stylus_utils::SetNoStylusInputForTesting();
  }

  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    root_window_controller->GetStatusAreaWidget()
        ->palette_tray()
        ->SetDisplayHasStylusForTesting();
  }
}

void StatusAreaInternalsHandler::ToggleLogoutTray(bool visible) {
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(prefs::kShowLogoutButtonInTray, visible);
}

void StatusAreaInternalsHandler::ToggleVirtualKeyboardTray(bool visible) {
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, visible);
}

void StatusAreaInternalsHandler::ToggleDictationTray(bool visible) {
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(prefs::kAccessibilityDictationEnabled, visible);
}

void StatusAreaInternalsHandler::ToggleVideoConferenceTray(bool visible) {
  VideoConferenceMediaState state;

  if (!visible) {
    VideoConferenceTrayController::Get()->UpdateWithMediaState(state);
    return;
  }

  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  VideoConferenceTrayController::Get()->UpdateWithMediaState(state);
}

void StatusAreaInternalsHandler::ToggleAnnotationTray(bool visible) {
  auto* root_window_controller = Shell::Get()->GetPrimaryRootWindowController();
  DCHECK(root_window_controller);
  DCHECK(root_window_controller->GetStatusAreaWidget());

  auto* annotator_controller = Shell::Get()->annotator_controller();

  if (visible) {
    annotator_controller->RegisterView(
        /*current_root=*/root_window_controller->GetRootWindow());
  } else {
    annotator_controller->DisableAnnotator();
  }
}

void StatusAreaInternalsHandler::TriggerPrivacyIndicators(
    const std::string& app_id,
    const std::string& app_name,
    bool is_camera_used,
    bool is_microphone_used) {
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, base::UTF8ToUTF16(app_name), is_camera_used, is_microphone_used,
      base::MakeRefCounted<PrivacyIndicatorsNotificationDelegate>(),
      PrivacyIndicatorsSource::kApps);
}

void StatusAreaInternalsHandler::SetIsInUserChildSession(
    bool in_child_session) {
  scoped_fake_model_->fake_model()->set_is_in_user_child_session(
      in_child_session);
}

void StatusAreaInternalsHandler::ResetHmrConsentStatus() {
  chromeos::MagicBoostState::Get()->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kUnset);
}

void StatusAreaInternalsHandler::SetBatteryIcon(
    const PageHandler::BatteryIcon icon) {
  FakePowerStatus* fake_power_status =
      scoped_fake_power_status_->fake_power_status();
  fake_power_status->SetDefaultState();
  switch (icon) {
    case PageHandler::BatteryIcon::kXIcon:
      fake_power_status->SetIsBatteryPresent(false);
      break;
    case PageHandler::BatteryIcon::kUnreliableIcon:
      fake_power_status->SetIsUsbChargerConnected(true);
      break;
    case PageHandler::BatteryIcon::kBoltIcon:
      fake_power_status->SetIsLinePowerConnected(true);
      break;
    case PageHandler::BatteryIcon::kBatterySaverPlusIcon:
      fake_power_status->SetIsBatterySaverActive(true);
      break;
    default:
      break;
  }
}

void StatusAreaInternalsHandler::SetBatteryPercent(double percent) {
  scoped_fake_power_status_->fake_power_status()->SetBatteryPercent(percent);
}

}  // namespace ash
