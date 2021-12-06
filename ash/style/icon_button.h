// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ICON_BUTTON_H_
#define ASH_STYLE_ICON_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A circular ImageButton that can have small/medium/large different sizes. Each
// of them has the floating version, which does not have the background. The
// button can be togglable if `is_togglable` is set to true, the icon inside
// might change on different toggle states. A fixed size of EmptyBorder will be
// applied to the button if `has_border` is true, this is done to help
// differentiating focus ring from the content of the button.
class IconButton : public views::ImageButton {
 public:
  METADATA_HEADER(IconButton);

  enum class Type {
    kSmall,
    kMedium,
    kLarge,
    kSmallFloating,
    kMediumFloating,
    kLargeFloating
  };

  // Used to determine how the button will behave when disabled.
  enum class DisabledButtonBehavior {
    // The button will display toggle button as off.
    kNone = 0,

    // The button will display on/off status of toggle.
    kCanDisplayDisabledToggleValue = 1,
  };

  IconButton(PressedCallback callback,
             Type type,
             const gfx::VectorIcon* icon,
             int accessible_name_id);
  IconButton(PressedCallback callback,
             Type type,
             const gfx::VectorIcon* icon,
             bool is_togglable,
             bool has_border);
  IconButton(PressedCallback callback,
             Type type,
             const gfx::VectorIcon* icon,
             int accessible_name_id,
             bool is_togglable,
             bool has_border);

  IconButton(const IconButton&) = delete;
  IconButton& operator=(const IconButton&) = delete;
  ~IconButton() override;

  bool toggled() const { return toggled_; }

  void set_button_behavior(DisabledButtonBehavior button_behavior) {
    button_behavior_ = button_behavior;
  }

  // Sets the vector icon of the button, it might change on different `toggled_`
  // states.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Updates the `toggled_` state of the button.
  void SetToggled(bool toggled);

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 protected:
  void UpdateVectorIcon();

 private:
  // For unit tests.
  friend class BluetoothFeaturePodControllerTest;

  const Type type_;
  const gfx::VectorIcon* icon_ = nullptr;

  // True if this button is togglable.
  bool is_togglable_ = false;

  // True if the button is currently toggled.
  bool toggled_ = false;

  DisabledButtonBehavior button_behavior_ = DisabledButtonBehavior::kNone;
};

}  // namespace ash

#endif  // ASH_STYLE_ICON_BUTTON_H_
