// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BUTTON_DESK_BUTTON_H_
#define ASH_WM_DESKS_DESK_BUTTON_DESK_BUTTON_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/shelf/shelf.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"

namespace views {
class ImageView;
class Label;
}

namespace ash {

class Desk;
class DeskButtonContainer;

// A button that allows for traversal between virtual desks. This button has two
// smaller buttons inside of it that appear when the button is hovered. These
// buttons allow for traversal to the left or right desk, while clicking on the
// button itself opens up a desk bar.
class ASH_EXPORT DeskButton : public views::Button {
  METADATA_HEADER(DeskButton, views::Button)

 public:
  DeskButton();
  DeskButton(const DeskButton&) = delete;
  DeskButton& operator=(const DeskButton&) = delete;
  ~DeskButton() override;

  views::Label* desk_name_label() const { return desk_name_label_; }

  bool is_activated() const { return is_activated_; }

  bool zero_state() const { return zero_state_; }

  void SetZeroState(bool zero_state);

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // Initializes the view. Must be called before any meaningful UIs can be laid
  // out.
  void Init(DeskButtonContainer* desk_button_container);

  void SetActivation(bool is_activated);

  std::u16string GetTitle() const;

  // Returns the button insets given current button state.
  gfx::Insets GetButtonInsets() const;

  // Updates UI status without re-layout.
  void UpdateUi(const Desk* active_desk);

  // Returns true if it is currently showing the desk profile avatar.
  bool IsShowingAvatar() const;

  void UpdateAvatar(const Desk* active_desk);

  // Updates locale-specific settings.
  void UpdateLocaleSpecificSettings();

 private:
  enum class SwitchButtonUpdateSource {
    // Used to update switch button before switching to the previous desk.
    kPreviousButtonPressed = -1,
    // Used to update switch button when there is no switch.
    kDeskButtonUpdate = 0,
    // Used to update switch button before switching to the next desk.
    kNextButtonPressed = 1,
  };

  // Toggles the button's `is_activated_` state and adjusts the button's style
  // to reflect the new activation state.
  void OnButtonPressed();

  std::u16string GetDeskNameLabelText(const Desk* active_desk) const;

  // Updates the shelf auto-hide disabler given `should_enable_shelf_auto_hide`.
  void UpdateShelfAutoHideDisabler(
      std::optional<Shelf::ScopedDisableAutoHide>& disabler,
      bool should_enable_shelf_auto_hide);

  void UpdateBackground();

  // A view that displays the profile avatar of the current desk.
  raw_ptr<views::ImageView> desk_avatar_view_;

  // Image for the profile avatar.
  gfx::ImageSkia desk_avatar_image_;

  // Profile summary of the desk's associated profile. It's cached during
  // `UpdateAvatar()`.
  LacrosProfileSummary profile_;

  // A label that displays the active desk's name.
  raw_ptr<views::Label> desk_name_label_;

  raw_ptr<DeskButtonContainer> desk_button_container_;

  // Indicates if the button is zero state.
  bool zero_state_ = false;

  // Tracks whether the button is currently activated (i.e. whether the desk
  // button has been pressed).
  bool is_activated_ = false;

  // Used to suspend the shelf from auto-hiding when the button is activated.
  std::optional<Shelf::ScopedDisableAutoHide>
      disable_shelf_auto_hide_activation_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, DeskButton, views::Button)
VIEW_BUILDER_METHOD(Init, DeskButtonContainer*)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ash::DeskButton)

#endif  // ASH_WM_DESKS_DESK_BUTTON_DESK_BUTTON_H_
