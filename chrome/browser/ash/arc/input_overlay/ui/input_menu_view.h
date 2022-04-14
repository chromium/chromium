// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

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
      views::View* entry_view);

  InputMenuView(DisplayOverlayController* display_overlay_controller,
                views::View* entry_view);

  InputMenuView(const InputMenuView&) = delete;
  InputMenuView& operator=(const InputMenuView&) = delete;
  ~InputMenuView() override;

 private:
  class FeedbackButton;

  void CloseMenu();
  void Init();
  std::unique_ptr<views::View> BuildSeparator();

  void OnToggleGameControlPressed();
  void OnToggleShowHintPressed();
  void OnButtonCustomizedPressed();
  void OnButtonSendFeedbackPressed();
  // Calculate Insets for a given |view|, taking into account specs and hosted
  // views. This is a fix due to the lack of a justify option for FlexLayout.
  gfx::Insets CalculateInsets(views::View* view,
                              int left,
                              int right,
                              int other_spacing) const;

  raw_ptr<views::ToggleButton> game_control_toggle_ = nullptr;
  raw_ptr<views::ToggleButton> show_hint_toggle_ = nullptr;
  raw_ptr<ash::PillButton> customize_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  // Kept around to determine bounds(), not owned.
  const raw_ptr<views::View> entry_view_ = nullptr;

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_
