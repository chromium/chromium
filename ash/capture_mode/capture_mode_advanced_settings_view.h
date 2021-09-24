// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_ADVANCED_SETTINGS_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_ADVANCED_SETTINGS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Separator;
}  // namespace views

namespace ash {

class CaptureModeBarView;

// TODO(conniekxu): This will replace CaptureModeSettingsView once
// feature 'ImprovedScreenCaptureSettings' is fully launched.
// A view that acts as the content view of the capture mode settings menu
// widget. It is the content view of settings widget and it contains
// `CaptureModeMenuGroup` for each setting, save to, audio input etc.
class ASH_EXPORT CaptureModeAdvancedSettingsView
    : public views::View,
      public CaptureModeMenuGroup::Delegate {
 public:
  METADATA_HEADER(CaptureModeAdvancedSettingsView);

  CaptureModeAdvancedSettingsView();
  CaptureModeAdvancedSettingsView(const CaptureModeAdvancedSettingsView&) =
      delete;
  CaptureModeAdvancedSettingsView& operator=(
      const CaptureModeAdvancedSettingsView&) = delete;
  ~CaptureModeAdvancedSettingsView() override;

  // Gets the ideal bounds in screen coordinates of the settings widget on
  // the given 'capture_mode_bar_view'.
  static gfx::Rect GetBounds(CaptureModeBarView* capture_mode_bar_view);

  // CaptureModeMenuGroup::Delegate:
  void OnOptionSelected(int option_id) const override;
  bool IsOptionChecked(int option_id) const override;

  // For tests only:
  CaptureModeMenuGroup* GetAudioInputMenuGroupForTesting() {
    return audio_input_menu_group_;
  }
  views::View* GetMicrophoneOptionForTesting();
  views::View* GetOffOptionForTesting();

 private:
  // TODO(afakhry|conniekxu): This is the callback function on menu item click.
  // It will be only used by the menu item in |save_to_menu_| for now. It should
  // open the folder window for user to select a folder to save the captured
  // files to.
  void HandleMenuClick();

  // "Audio input" menu group that users can select an audio input from for
  // screen capture recording. It has "Off" and "Microphone" options for now.
  // "Off" is the default one which means no audio input selected.
  CaptureModeMenuGroup* audio_input_menu_group_;

  views::Separator* separator_;

  // "Save to" menu group that users can select a folder to save the captured
  // files to. It will include the "Downloads" folder as the default one and
  // one more folder selected by users.
  CaptureModeMenuGroup* save_to_menu_group_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_ADVANCED_SETTINGS_VIEW_H_