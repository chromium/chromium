// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_advanced_settings_test_api.h"

#include "ash/capture_mode/capture_mode_advanced_settings_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/constants/ash_features.h"
#include "base/check.h"

namespace ash {

namespace {

CaptureModeSession* GetCaptureModeSession() {
  DCHECK(features::AreImprovedScreenCaptureSettingsEnabled());
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  CaptureModeSession* session = controller->capture_mode_session();
  DCHECK(session->capture_mode_settings_widget());
  return session;
}

}  // namespace

CaptureModeAdvancedSettingsTestApi::CaptureModeAdvancedSettingsTestApi()
    : settings_view_(
          GetCaptureModeSession()->capture_mode_advanced_settings_view_) {}

CaptureModeAdvancedSettingsView*
CaptureModeAdvancedSettingsTestApi::GetAdvancedSettingsView() {
  return settings_view_;
}

CaptureModeMenuGroup*
CaptureModeAdvancedSettingsTestApi::GetAudioInputMenuGroup() {
  return settings_view_->audio_input_menu_group_;
}

views::View* CaptureModeAdvancedSettingsTestApi::GetMicrophoneOption() {
  return GetAudioInputMenuGroup()->GetOptionForTesting(kAudioMicrophone);
}

views::View* CaptureModeAdvancedSettingsTestApi::GetAudioOffOption() {
  return GetAudioInputMenuGroup()->GetOptionForTesting(kAudioOff);
}

CaptureModeMenuGroup* CaptureModeAdvancedSettingsTestApi::GetSaveToMenuGroup() {
  return settings_view_->save_to_menu_group_;
}

views::View* CaptureModeAdvancedSettingsTestApi::GetDefaultDownloadsOption() {
  return GetSaveToMenuGroup()->GetOptionForTesting(kDownloadsFolder);
}

views::View* CaptureModeAdvancedSettingsTestApi::GetCustomFolderOptionIfAny() {
  return GetSaveToMenuGroup()->GetOptionForTesting(kCustomFolder);
}

views::View* CaptureModeAdvancedSettingsTestApi::GetSelectFolderMenuItem() {
  return GetSaveToMenuGroup()->GetSelectFolderMenuItemForTesting();
}

void CaptureModeAdvancedSettingsTestApi::SetOnSettingsMenuRefreshedCallback(
    base::OnceClosure callback) {
  settings_view_->on_settings_menu_refreshed_callback_for_test_ =
      std::move(callback);
}

}  // namespace ash
