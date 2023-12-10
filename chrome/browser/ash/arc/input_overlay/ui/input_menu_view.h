// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
//   | Game Controls |Alpha| [ o]  [x] |
//   |                                 |
//   | Key mapping             [Edit]  |
//   |                                 |
//   | Show key mapping          [ o]  |
//   |                                 |
//   | Send feedback                   |
//   +---------------------------------+

namespace arc::input_overlay {

class DisplayOverlayController;

class InputMenuView : public views::View {
  METADATA_HEADER(InputMenuView, views::View)

 public:
  static std::unique_ptr<InputMenuView> BuildMenuView(
      DisplayOverlayController* display_overlay_controller,
      views::View* entry_view,
      const gfx::Size& parent_size);

  InputMenuView(DisplayOverlayController* display_overlay_controller,
                views::View* entry_view);

  InputMenuView(const InputMenuView&) = delete;
  InputMenuView& operator=(const InputMenuView&) = delete;
  ~InputMenuView() override;

  // views::View:
  void OnThemeChanged() override;

 private:
  class FeedbackButton;

  void CloseMenu();
  void Init(const gfx::Size& parent_size);
  std::unique_ptr<views::View> BuildSeparator();

  void OnToggleGameControlPressed();
  void OnToggleShowHintPressed();
  void OnEditButtonPressed();
  void OnButtonSendFeedbackPressed();
  // Calculate Insets for a given `view`, taking into account specs and hosted
  // views. This is a fix due to the lack of a justify option for FlexLayout.
  gfx::Insets CalculateInsets(views::View* view,
                              int left,
                              int right,
                              int other_spacing,
                              int menu_width) const;
  // Set `toggle` colors to spec.
  void SetCustomToggleColor(views::ToggleButton* toggle);

  raw_ptr<views::ToggleButton> game_control_toggle_ = nullptr;
  raw_ptr<views::ToggleButton> show_mapping_toggle_ = nullptr;
  raw_ptr<ash::PillButton> edit_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  // Kept around to determine bounds(), not owned.
  const raw_ptr<views::View> entry_view_ = nullptr;

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MENU_VIEW_H_
