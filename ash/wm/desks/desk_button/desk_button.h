// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BUTTON_DESK_BUTTON_H_
#define ASH_WM_DESKS_DESK_BUTTON_DESK_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf.h"
#include "ash/shell_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace views {
class Label;
}

namespace ash {

class DeskButtonWidget;

// Buttons that can be clicked to switch to the left or right desk.
class ASH_EXPORT DeskSwitchButton : public views::ImageButton {
 public:
  METADATA_HEADER(DeskSwitchButton);

  explicit DeskSwitchButton(PressedCallback callback);
  DeskSwitchButton(const DeskSwitchButton&) = delete;
  DeskSwitchButton& operator=(const DeskSwitchButton&) = delete;
  ~DeskSwitchButton() override;

  void set_hovered(bool hovered) { hovered_ = hovered; }

  // Sets opacity and enabled state based on the input `show` state.
  void SetShown(bool show);
  bool GetShown() const;

 private:
  // views::ImageButton:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // views::View:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // views::ViewObserver:
  void OnViewBlurred(views::View* observed_view) override;

  bool hovered_ = false;
};

// A button that allows for traversal between virtual desks. This button has two
// smaller buttons inside of it that appear when the button is hovered. These
// buttons allow for traversal to the left or right desk, while clicking on the
// button itself opens up a desk bar.
class ASH_EXPORT DeskButton : public views::Button,
                              public DesksController::Observer,
                              public ShellObserver {
 public:
  METADATA_HEADER(DeskButton);

  explicit DeskButton(DeskButtonWidget* desk_button_widget);
  DeskButton(const DeskButton&) = delete;
  DeskButton& operator=(const DeskButton&) = delete;
  ~DeskButton() override;

  bool is_activated() const { return is_activated_; }
  DeskSwitchButton* prev_desk_button() const { return prev_desk_button_; }
  DeskSwitchButton* next_desk_button() const { return next_desk_button_; }

  void set_force_expanded_state(bool force_expanded_state) {
    force_expanded_state_ = force_expanded_state;
  }

  // Updates label text and visibility of children.
  void OnExpandedStateUpdate(bool expanded);

  bool GetHovered() const;

  void SetActivation(bool is_activated);

  void SetFocused(bool is_focused);

  // Changes whether the button is expanded and if the switch buttons are shown
  // depending on if the button is focused.
  void MaybeContract();

  std::u16string GetTitleForView(const views::View* view);

  views::Label* desk_name_label_for_test() const { return desk_name_label_; }
  bool is_expanded_for_test() const { return is_expanded_; }

  const std::u16string& GetTextForTest() const;

 private:
  enum class SwitchButtonUpdateSource {
    // Used to update switch button before switching to the previous desk.
    kPreviousButtonPressed = -1,
    // Used to update switch button when there is no switch.
    kDeskButtonUpdate = 0,
    // Used to update switch button before switching to the next desk.
    kNextButtonPressed = 1,
  };

  // views::Button:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void StateChanged(ButtonState old_state) override;
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk, bool from_undo) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override;

  // views::View:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // views::ViewObserver:
  void OnViewBlurred(views::View* observed_view) override;

  // ShellObserver:
  void OnShellDestroying() override;

  // Toggles the button's `is_activated_` state and adjusts the button's style
  // to reflect the new activation state.
  void OnButtonPressed();

  // Switches to the desk to the left of the active desk.
  void OnPreviousPressed();

  // Switches to the desk to the right of the active desk.
  void OnNextPressed();

  // Determines how we can present the currently active desk name and updates
  // `desk_name_` and `abbreviated_desk_name_` accordingly.
  void CalculateDisplayNames(const Desk* desk);

  // Determines whether the desk switch buttons can be shown. `source` is used
  // to calculate target active desk index before desk switch.
  void MaybeUpdateDeskSwitchButtonVisibility(SwitchButtonUpdateSource source);

  // Shows the context menu for the given located event when the button is not
  // activated. Please note, it re-uses the shelf view as the context menu
  // controller so that they show the same menu items.
  void MaybeShowContextMenuForEvent(ui::LocatedEvent* event);

  // Updates the shelf auto-hide disabler given `should_enable_shelf_auto_hide`.
  void UpdateShelfAutoHideDisabler(
      std::optional<Shelf::ScopedDisableAutoHide>& disabler,
      bool should_enable_shelf_auto_hide);

  // Set up the focus ring, focus behavior, and highlight path for the buttons.
  void SetupFocus(views::Button* view);

  // Widget that maintains this object.
  // TODO(b/272383056): Remove this and this class's dependence on accessing it.
  raw_ptr<DeskButtonWidget> desk_button_widget_;

  // Button that switches to the desk to the left of the active desk.
  raw_ptr<DeskSwitchButton> prev_desk_button_;

  // A label that displays the active desk's name.
  raw_ptr<views::Label> desk_name_label_;

  // Button that switches to the desk to the right of the active desk.
  raw_ptr<DeskSwitchButton> next_desk_button_;

  // The name and abbreviated name of the currently active desk.
  std::u16string desk_name_;
  std::u16string abbreviated_desk_name_;

  // Tracks whether the button is currently in expanded state.
  bool is_expanded_ = false;

  // Tracks whether the button is currently activated (i.e. whether the desk
  // button has been pressed).
  bool is_activated_ = false;

  // Tracks whether the button currently has focus.
  bool is_focused_ = false;

  // Indicates that the shelf is horizontal and therefore the button should
  // always be expanded.
  bool force_expanded_state_ = false;

  // Indicates that shell is destroying.
  bool is_shell_destroying_ = false;

  // Used to suspend the shelf from audo-hiding when the button is activated or
  // hovered.
  std::optional<Shelf::ScopedDisableAutoHide>
      disable_shelf_auto_hide_activation_;
  std::optional<Shelf::ScopedDisableAutoHide> disable_shelf_auto_hide_hover_;
};

}  // namespace ash

#endif
