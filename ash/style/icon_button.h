// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ICON_BUTTON_H_
#define ASH_STYLE_ICON_BUTTON_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
class ImageSkia;
struct VectorIcon;
}  // namespace gfx

namespace ui {
class Event;
}  // namespace ui

namespace ash {

class BlurredBackgroundShield;

// A circular ImageButton that can have small/medium/large different sizes. Each
// of them has the floating version, which does not have the background. The
// prominent-floating buttons have different icon colors when the button is
// focused and unfocused. The button can be togglable if `is_togglable` is set
// to true, the icon inside might change on different toggle states. A fixed
// size of EmptyBorder will be applied to the button if `has_border` is true,
// this is done to help differentiating focus ring from the content of the
// button.
class ASH_EXPORT IconButton : public views::ImageButton {
 public:
  METADATA_HEADER(IconButton);

  using ColorVariant = absl::variant<SkColor, ui::ColorId>;

  enum class Type {
    kXSmall,
    kSmall,
    kMedium,
    kLarge,
    kXSmallProminent,
    kSmallProminent,
    kMediumProminent,
    kLargeProminent,
    kXSmallFloating,
    kSmallFloating,
    kMediumFloating,
    kLargeFloating,
    kXSmallProminentFloating,
    kSmallProminentFloating,
    kMediumProminentFloating,
    kLargeProminentFloating,
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
             const std::u16string& accessible_name,
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

  void SetButtonBehavior(DisabledButtonBehavior button_behavior);

  // Sets the vector icon of the button, it might change on different `toggled_`
  // states.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Sets the vector icon used when the button is toggled. If the button does
  // not specify a toggled vector icon, it will use the same vector icon for
  // all states.
  void SetToggledVectorIcon(const gfx::VectorIcon& icon);

  // Sets the button's background color or toggled color with color value and
  // color ID when the button wants to have a different background color from
  // the default one. When both color value and color ID are set, color ID takes
  // the precedence.
  void SetBackgroundColor(ColorVariant background_color);
  void SetBackgroundToggledColor(ColorVariant background_toggled_color);

  // Sets the button's background image. The |background_image| is resized to
  // fit the button. Note, if set, |background_image| is painted on top of
  // the button's existing background color.
  void SetBackgroundImage(const gfx::ImageSkia& background_image);

  // Sets the button's icon color or toggled color with color value and color ID
  // when the button wants to have a different icon color from the default one.
  // When both color value and color ID are set, color ID takes the precedence.
  void SetIconColor(ColorVariant icon_color);
  void SetIconToggledColor(ColorVariant icon_toggled_color);

  // Sets the size to use for the vector icon in DIPs.
  void SetIconSize(int size);

  // Updates the `toggled_` state of the button.
  void SetToggled(bool toggled);

  // Sets whether to enable the blurred background shield. Setting blurred
  // background shield enabled will use a blurred background shield to replace
  // the current background. For floating type button with untoggled state,
  // there is no blurred background shield even it is enabled.
  void SetEnableBlurredBackgroundShield(bool enable);

  // views::ImageButton:
  void OnFocus() override;
  void OnBlur() override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void NotifyClick(const ui::Event& event) override;

 protected:
  void UpdateBackground();
  void UpdateBlurredBackgroundShield();
  void UpdateVectorIcon(bool color_changes_only = false);

  void OnEnabledStateChanged();

  // Gets the background color of the icon button.
  SkColor GetBackgroundColor() const;

 private:
  // For unit tests.
  friend class BluetoothFeaturePodControllerTest;

  // True if the button is in the state of toggled, even when the button is
  // disabled.
  bool IsToggledOn() const;

  const Type type_;
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> icon_ = nullptr;
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> toggled_icon_ = nullptr;

  // True if this button is togglable.
  bool is_togglable_ = false;

  // True if the button is currently toggled.
  bool toggled_ = false;

  // Background colors and icon colors.
  ColorVariant background_color_ = gfx::kPlaceholderColor;
  ColorVariant background_toggled_color_ = gfx::kPlaceholderColor;
  ColorVariant icon_color_ = gfx::kPlaceholderColor;
  ColorVariant icon_toggled_color_ = gfx::kPlaceholderColor;

  bool blurred_background_shield_enabled_ = false;
  // Note: the blurred background shield will still be null if the button type
  // is floating with untoggled state.
  std::unique_ptr<BlurredBackgroundShield> blurred_background_shield_;

  // Custom value for icon size (usually used to make the icon smaller).
  std::optional<int> icon_size_;

  // Called to update background color when the button is enabled/disabled.
  base::CallbackListSubscription enabled_changed_subscription_;

  DisabledButtonBehavior button_behavior_ = DisabledButtonBehavior::kNone;
};

}  // namespace ash

#endif  // ASH_STYLE_ICON_BUTTON_H_
