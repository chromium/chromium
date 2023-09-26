// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/status_area_internals/status_area_internals_handler.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"

namespace ash {

StatusAreaInternalsHandler::StatusAreaInternalsHandler(
    mojo::PendingReceiver<mojom::status_area_internals::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

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

void StatusAreaInternalsHandler::ToggleProjectorTray(bool visible) {
  auto* root_window_controller = Shell::Get()->GetPrimaryRootWindowController();
  DCHECK(root_window_controller);
  DCHECK(root_window_controller->GetStatusAreaWidget());

  auto* projector_ui_controller =
      Shell::Get()->projector_controller()->ui_controller();

  if (visible) {
    projector_ui_controller->ShowAnnotationTray(
        /*current_root=*/root_window_controller->GetRootWindow());
  } else {
    projector_ui_controller->HideAnnotationTray();
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

}  // namespace ash
