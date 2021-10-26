// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_ADVANCED_SETTINGS_TEST_API_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_ADVANCED_SETTINGS_TEST_API_H_

#include "base/callback_forward.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class CaptureModeAdvancedSettingsView;
class CaptureModeMenuGroup;

// Test APIs to test the UI of the advanced settings menu. Can only be created
// while a capture mode session is active, and the settings menu is shown.
class CaptureModeAdvancedSettingsTestApi {
 public:
  CaptureModeAdvancedSettingsTestApi();
  CaptureModeAdvancedSettingsTestApi(
      const CaptureModeAdvancedSettingsTestApi&) = delete;
  CaptureModeAdvancedSettingsTestApi& operator=(
      const CaptureModeAdvancedSettingsTestApi&) = delete;
  ~CaptureModeAdvancedSettingsTestApi() = default;

  // Returns the content view of the advanced settings widget.
  CaptureModeAdvancedSettingsView* GetAdvancedSettingsView();

  // Returns the audio settings menu group and the views for its options.
  CaptureModeMenuGroup* GetAudioInputMenuGroup();
  views::View* GetMicrophoneOption();
  views::View* GetAudioOffOption();

  // Returns the save-to settings menu group and the views for its options.
  CaptureModeMenuGroup* GetSaveToMenuGroup();
  views::View* GetDefaultDownloadsOption();
  views::View* GetCustomFolderOptionIfAny();

  // Returns the view for the "Select folder" menu item which when pressed would
  // open the folder selection dialog.
  views::View* GetSelectFolderMenuItem();

  // Sets a callback that will be triggered once the settings menu is refreshed.
  void SetOnSettingsMenuRefreshedCallback(base::OnceClosure callback);

 private:
  // Valid only while the settings menu is shown.
  CaptureModeAdvancedSettingsView* const settings_view_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_ADVANCED_SETTINGS_TEST_API_H_
