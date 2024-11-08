// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_
#define ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class BoxLayout;
class ImageView;
class Label;
class Throbber;
}  // namespace views

namespace ash {

class SystemShadow;

// A view that displays an action button. The action button may show both an
// icon and text or be collapsed into just an icon.
class ASH_EXPORT ActionButtonView : public views::Button {
  METADATA_HEADER(ActionButtonView, views::Button)

 public:
  ActionButtonView(views::Button::PressedCallback callback,
                   std::u16string text,
                   const gfx::VectorIcon* icon,
                   ActionButtonRank rank);
  ActionButtonView(const ActionButtonView&) = delete;
  ActionButtonView& operator=(const ActionButtonView&) = delete;
  ~ActionButtonView() override;

  ActionButtonRank rank() const { return rank_; }

  void set_show_throbber_when_pressed(bool show_throbber_when_pressed) {
    show_throbber_when_pressed_ = show_throbber_when_pressed;
  }

  // views::Button:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;
  void OnEnabledChanged() override;
  void StateChanged(ButtonState old_state) override;

  // Collapses the action button, hiding its label so that only the icon
  // shows.
  void CollapseToIconButton();

  const views::ImageView* image_view_for_testing() const { return image_view_; }
  const views::Label* label_for_testing() const { return label_; }

 private:
  // Sets the color of the background and label, and sets a new icon based on
  // the enabled state.
  void UpdateColorsAndIcon();

  // Rank used to determine ordering of action buttons.
  const ActionButtonRank rank_;

  std::unique_ptr<SystemShadow> shadow_;

  raw_ptr<views::BoxLayout> box_layout_ = nullptr;

  // The icon to display in the image view.
  const raw_ptr<const gfx::VectorIcon> icon_;

  // The image view for the action button icon.
  raw_ptr<views::ImageView> image_view_ = nullptr;

  // The label containing the action button text. This label is hidden when the
  // action button is collapsed.
  raw_ptr<views::Label> label_ = nullptr;

  // Loading throbber shown when the button's action is in progress.
  // TODO(crbug.com/378023303): The loading throbber is only temporary and
  // should be removed once the finalized loading animation is implemented.
  raw_ptr<views::Throbber> throbber_ = nullptr;
  bool show_throbber_when_pressed_ = false;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_
