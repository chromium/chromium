// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_

#include "ui/views/view.h"

#include <memory>

namespace ash {
class PillButton;
}  // namespace ash

namespace views {
class ImageButton;
class ToggleButton;
}  // namespace views

// A view that shows display options for input overlay, this is the entry
// point for customizing key bindings and turning the feature on/off.
//
// The class reports back to DisplayOverlayController, who owns this.
//   +---------------------------------+
//   | Game Control       [ o]    [x]  |
//   |                                 |
//   | Key mapping        [Customize]  |
//   |                                 |
//   | Show hint overlay         [ o]  |
//   |                                 |
//   | Send feedback                   |
//   +---------------------------------+

namespace arc {

namespace input_overlay {
class DisplayOverlayController;
class InputMenuView : public views::View {
 public:
  static std::unique_ptr<InputMenuView> BuildMenuView(
      DisplayOverlayController* display_overlay_controller,
      views::View* anchor_view);

  // TODO(djacobo): Pass a callback to return responses to owner.
  InputMenuView(DisplayOverlayController* display_overlay_controller,
                views::View* anchor_view);

  InputMenuView(const InputMenuView&) = delete;
  InputMenuView& operator=(const InputMenuView&) = delete;
  ~InputMenuView() override;

 private:
  void CloseMenu();
  void Init();
  std::unique_ptr<views::View> BuildSeparator();

  void OnToggleGameControlPressed();
  void OnToggleShowHintPressed();
  void OnButtonCustomizedPressed();

  views::ToggleButton* game_control_toggle_ = nullptr;
  views::ToggleButton* show_hint_toggle_ = nullptr;
  ash::PillButton* customize_button_ = nullptr;
  views::ImageButton* close_button_ = nullptr;

  // Kept around to determine bounds(), not owned.
  views::View* const entry_view_ = nullptr;

  // DisplayOverlayController owns this class, no need to deallocate.
  DisplayOverlayController* const display_overlay_controller_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_
