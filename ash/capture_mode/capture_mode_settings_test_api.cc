// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_settings_test_api.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "base/check.h"

namespace ash {

namespace {

CaptureModeSession* GetCaptureModeSession() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  CaptureModeSession* session =
      static_cast<CaptureModeSession*>(controller->capture_mode_session());
  CHECK_EQ(session->session_type(), SessionType::kReal);
  DCHECK(session->capture_mode_settings_widget());
  return session;
}

}  // namespace

CaptureModeSettingsTestApi::CaptureModeSettingsTestApi()
    : settings_view_(GetCaptureModeSession()->capture_mode_settings_view_) {}

CaptureModeSettingsView* CaptureModeSettingsTestApi::GetSettingsView() {
  return settings_view_;
}

CaptureModeMenuGroup* CaptureModeSettingsTestApi::GetAudioInputMenuGroup() {
  return settings_view_->audio_input_menu_group_;
}

views::View* CaptureModeSettingsTestApi::GetMicrophoneOption() {
  return GetAudioInputMenuGroup()->GetOptionForTesting(kAudioMicrophone);
}

views::View* CaptureModeSettingsTestApi::GetAudioOffOption() {
  return GetAudioInputMenuGroup()->GetOptionForTesting(kAudioOff);
}

views::View* CaptureModeSettingsTestApi::GetSystemAudioOption() {
  return GetAudioInputMenuGroup()->GetOptionForTesting(kAudioSystem);
}

views::View* CaptureModeSettingsTestApi::GetSystemAndMicrophoneAudioOption() {
  return GetAudioInputMenuGroup()->GetOptionForTesting(
      kAudioSystemAndMicrophone);
}

CaptureModeMenuGroup* CaptureModeSettingsTestApi::GetSaveToMenuGroup() {
  return settings_view_->save_to_menu_group_;
}

views::View* CaptureModeSettingsTestApi::GetDefaultDownloadsOption() {
  return GetSaveToMenuGroup()->GetOptionForTesting(kDownloadsFolder);
}

views::View* CaptureModeSettingsTestApi::GetCustomFolderOptionIfAny() {
  return GetSaveToMenuGroup()->GetOptionForTesting(kCustomFolder);
}

views::View* CaptureModeSettingsTestApi::GetSelectFolderMenuItem() {
  return GetSaveToMenuGroup()->GetSelectFolderMenuItemForTesting();
}

CaptureModeMenuGroup* CaptureModeSettingsTestApi::GetCameraMenuGroup() {
  return settings_view_->camera_menu_group_;
}

views::View* CaptureModeSettingsTestApi::GetCameraMenuHeader() {
  return GetCameraMenuGroup()->menu_header();
}

views::View* CaptureModeSettingsTestApi::GetCameraOption(int option_id) {
  return GetCameraMenuGroup()->GetOptionForTesting(option_id);
}

void CaptureModeSettingsTestApi::SetOnSettingsMenuRefreshedCallback(
    base::OnceClosure callback) {
  settings_view_->on_settings_menu_refreshed_callback_for_test_ =
      std::move(callback);
}

CaptureModeMenuToggleButton*
CaptureModeSettingsTestApi::GetDemoToolsMenuToggleButton() {
  return settings_view_->demo_tools_menu_toggle_button_for_testing();
}

}  // namespace ash
