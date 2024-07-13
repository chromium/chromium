// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BUTTON_DESK_SWITCH_BUTTON_H_
#define ASH_WM_DESKS_DESK_BUTTON_DESK_SWITCH_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

class DeskButtonContainer;
class Desk;

// Buttons that can be clicked to switch to the left or right desk.
class ASH_EXPORT DeskSwitchButton : public views::ImageButton {
  METADATA_HEADER(DeskSwitchButton, views::ImageButton)

 public:
  enum class Type {
    kPrev,
    kNext,
  };

  DeskSwitchButton();
  DeskSwitchButton(const DeskSwitchButton&) = delete;
  DeskSwitchButton& operator=(const DeskSwitchButton&) = delete;
  ~DeskSwitchButton() override;

  Type type() { return type_; }

  // views::ImageButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void StateChanged(ButtonState old_state) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // Initializes the view. Must be called before any meaningful UIs can be laid
  // out.
  void Init(DeskButtonContainer* desk_button_container, Type type);

  std::u16string GetTitle() const;

  // Updates UI status without re-layout.
  void UpdateUi(const Desk* active_desk);

  // Updates locale-specific settings.
  void UpdateLocaleSpecificSettings();

 private:
  void DeskSwitchButtonPressed();

  // Shows or hides the background.
  void SetBackgroundVisible(bool visible);

  Type type_ = Type::kPrev;

  raw_ptr<DeskButtonContainer> desk_button_container_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, DeskSwitchButton, views::ImageButton)
VIEW_BUILDER_METHOD(Init, DeskButtonContainer*, DeskSwitchButton::Type)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ash::DeskSwitchButton)

#endif  // ASH_WM_DESKS_DESK_BUTTON_DESK_SWITCH_BUTTON_H_
