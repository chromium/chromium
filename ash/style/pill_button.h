// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_PILL_BUTTON_H_
#define ASH_STYLE_PILL_BUTTON_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

class BlurredBackgroundShield;

// A label button with a rounded rectangle background. It can have an icon
// inside as well, and its text and background colors will be different based on
// the type of the button.
class ASH_EXPORT PillButton : public views::LabelButton {
  METADATA_HEADER(PillButton, views::LabelButton)

 public:
  using ColorVariant = absl::variant<SkColor, ui::ColorId>;

  static constexpr int kPillButtonHorizontalSpacing = 16;
  static constexpr int kPaddingReductionForIcon = 4;

  // The PillButton style is defined with 4 features:
  // 1. Color variant defines which background, text, and icon color scheme to
  // be used, for example Default, Floating, Alert, etc.
  // 2. Button size indicates whether we should use the default size 32 or a
  // large size 36.
  // 3. With/without an icon.
  // 4. Icon position: leading or following.
  // For ease of extracting features from a button type, each feature is
  // represented by a different bit mask.
  using TypeFlag = int;

  static constexpr TypeFlag kDefault = 1;
  static constexpr TypeFlag kDefaultElevated = 1 << 1;
  static constexpr TypeFlag kPrimary = 1 << 2;
  static constexpr TypeFlag kSecondary = 1 << 3;
  static constexpr TypeFlag kFloating = 1 << 4;
  static constexpr TypeFlag kAlert = 1 << 5;
  // TODO(crbug.com/1355517): Get rid of `kAccent` after CrosNext is fully
  // launched.
  static constexpr TypeFlag kAccent = 1 << 6;
  static constexpr TypeFlag kLarge = 1 << 7;
  static constexpr TypeFlag kIconLeading = 1 << 8;
  static constexpr TypeFlag kIconFollowing = 1 << 9;

  // Types of the PillButton. Each type is represented as the bitwise OR
  // operation of the feature bit masks. The naming rule of the button type is
  // k{Color Variant}{Button Size}{Icon}{Icon Position}.
  enum Type {
    // PillButton with default text and background colors, a leading icon.
    kDefaultWithIconLeading = kDefault | kIconLeading,
    // PillButton with default text and background colors, a following icon.
    kDefaultWithIconFollowing = kDefault | kIconFollowing,
    // PillButton with default text and background colors, a large button size,
    // a leading icon.
    kDefaultLargeWithIconLeading = kDefault | kLarge | kIconLeading,
    // PillButton with default text and background colors, a large button size,
    // a following icon.
    kDefaultLargeWithIconFollowing = kDefault | kLarge | kIconFollowing,
    // PillButton with default text and background colors, no icon.
    kDefaultWithoutIcon = kDefault,
    // PillButton with default text and background colors, a large button size,
    // no icon.
    kDefaultLargeWithoutIcon = kDefault | kLarge,

    // PillButton with default-elevated text and background colors, a leading
    // icon.
    kDefaultElevatedWithIconLeading = kDefaultElevated | kIconLeading,
    // PillButton with default-elevated text and background colors, a following
    // icon.
    kDefaultElevatedWithIconFollowing = kDefaultElevated | kIconFollowing,
    // PillButton with default-elevated text and background colors, a large
    // button size, a leading icon.
    kDefaultElevatedLargeWithIconLeading =
        kDefaultElevated | kLarge | kIconLeading,
    // PillButton with default-elevated text and background colors, a large
    // button size, a following icon.
    kDefaultElevatedLargeWithIconFollowing =
        kDefaultElevated | kLarge | kIconFollowing,
    // PillButton with default-elevated text and background colors, no icon.
    kDefaultElevatedWithoutIcon = kDefaultElevated,
    // PillButton with default-elevated text and background colors, a large
    // button size,
    // no icon.
    kDefaultElevatedLargeWithoutIcon = kDefaultElevated | kLarge,

    // PillButton with primary text and background colors, a leading icon.
    kPrimaryWithIconLeading = kPrimary | kIconLeading,
    // PillButton with primary text and background colors, a following icon.
    kPrimaryWithIconFollowing = kPrimary | kIconFollowing,
    // PillButton with primary text and background colors, a large button size,
    // a leading icon.
    kPrimaryLargeWithIconLeading = kPrimary | kLarge | kIconLeading,
    // PillButton with primary text and background colors, a large button size,
    // a following icon.
    kPrimaryLargeWithIconFollowing = kPrimary | kLarge | kIconFollowing,
    // PillButton with primary text and background colors, no icon.
    kPrimaryWithoutIcon = kPrimary,
    // PillButton with primary text and background colors, a large button size,
    // no icon.
    kPrimaryLargeWithoutIcon = kPrimary | kLarge,

    // PillButton with secondary text and background colors, a leading icon.
    kSecondaryWithIconLeading = kSecondary | kIconLeading,
    // PillButton with secondary text and background colors, a following icon.
    kSecondaryWithIconFollowing = kSecondary | kIconFollowing,
    // PillButton with secondary text and background colors, a large button
    // size, a leading icon.
    kSecondaryLargeWithIconLeading = kSecondary | kLarge | kIconLeading,
    // PillButton with secondary text and background colors, a large button
    // size, a following icon.
    kSecondaryLargeWithIconFollowing = kSecondary | kLarge | kIconFollowing,
    // PillButton with secondary text and background colors, no icon.
    kSecondaryWithoutIcon = kSecondary,
    // PillButton with secondary text and background colors, a large button
    // size, no icon.
    kSecondaryLargeWithoutIcon = kSecondary | kLarge,

    // PillButton with floating text colors, no background, a leading icon.
    kFloatingWithIconLeading = kFloating | kIconLeading,
    // PillButton with floating text colors, no background, a following icon.
    kFloatingWithIconFollowing = kFloating | kIconFollowing,
    // PillButton with floating text colors, no background, a large button size,
    // a leading icon.
    kFloatingLargeWithIconLeading = kFloating | kLarge | kIconLeading,
    // PillButton with floating text colors, no background, a large button size,
    // a following icon.
    kFloatingLargeWithIconFollowing = kFloating | kLarge | kIconFollowing,
    // PillButton with floating text colors, no background, no icon.
    kFloatingWithoutIcon = kFloating,
    // PillButton with floating text colors, no background, a large button size,
    // no icon.
    kFloatingLargeWithoutIcon = kFloating | kLarge,

    // PillButton with alert text and background colors, a leading icon.
    kAlertWithIconLeading = kAlert | kIconLeading,
    // PillButton with alert text and background colors, a following icon.
    kAlertWithIconFollowing = kAlert | kIconFollowing,
    // PillButton with alert text and background colors, a large button size, a
    // leading icon.
    kAlertLargeWithIconLeading = kAlert | kLarge | kIconLeading,
    // PillButton with alert text and background colors, a large button size, a
    // following icon.
    kAlertLargeWithIconFollowing = kAlert | kLarge | kIconFollowing,
    // PillButton with alert text and background colors, no icon.
    kAlertWithoutIcon = kAlert,
    // PillButton with alert text and background colors, a large button size, no
    // icon.
    kAlertLargeWithoutIcon = kAlert | kLarge,

    // Old button types.
    // TODO(crbug.com/1355517): Get rid of these types after CrosNext is fully
    // launched.
    // PillButton with accent text and background colors, no icon.
    kAccentWithoutIcon = kAccent,
    // PillButton with accent text, no background, no icon.
    kAccentFloatingWithoutIcon = kAccent | kFloating,
  };

  explicit PillButton(
      PressedCallback callback = PressedCallback(),
      const std::u16string& text = std::u16string(),
      Type type = Type::kDefaultWithoutIcon,
      const gfx::VectorIcon* icon = nullptr,
      int horizontal_spacing = kPillButtonHorizontalSpacing,
      int padding_reduction_for_icon = kPaddingReductionForIcon);
  PillButton(const PillButton&) = delete;
  PillButton& operator=(const PillButton&) = delete;
  ~PillButton() override;

  // views::LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Insets GetInsets() const override;
  void UpdateBackgroundColor() override;
  views::PropertyEffects UpdateStyleToIndicateDefaultStatus() override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // Sets the button's background color, text's color or icon's color. Note, do
  // this only when the button wants to have different colors from the default
  // ones.
  void SetBackgroundColor(const SkColor background_color);
  void SetBackgroundColorId(ui::ColorId background_color_id);
  void SetButtonTextColor(const SkColor text_color);
  void SetButtonTextColorId(ui::ColorId text_color_id);
  void SetIconColor(const SkColor icon_color);
  void SetIconColorId(ui::ColorId icon_color_id);
  // TODO(b/290639214): This method is deprecating. Try not to change button
  // type afterward. If a new button type is needed, please create a new
  // instance.
  void SetPillButtonType(Type type);

  // Sets the button's label to use the default label font, which is smaller
  // and less heavily weighted.
  void SetUseDefaultLabelFont();

  // Sets if the button should enable the background blur. Once the button
  // enables the background blur, it will use `BlurredBackgroundShield` as the
  // background which is performance consuming so only use it as needed.
  void SetEnableBackgroundBlur(bool enable);

  void SetTextWithStringId(int message_id);
  void SetUseLabelAsDefaultTooltip(bool use_label_as_default_tooltip);

 private:
  // Initializes the button layout, focus ring and background according to the
  // button type.
  void Init();

  void UpdateTextColor();
  void UpdateIconColor();

  // Returns the spacing on the side where the icon locates. The value is set
  // smaller to make the spacing on two sides visually look the same.
  int GetHorizontalSpacingWithIcon() const;

  Type type_;
  const raw_ptr<const gfx::VectorIcon> icon_;

  // Horizontal spacing of this button. `kPillButtonHorizontalSpacing` will be
  // set as the default value.
  int horizontal_spacing_;

  // The padding reduced by icon.
  int padding_reduction_for_icon_;

  // Custom colors and color IDs.
  ColorVariant background_color_ = gfx::kPlaceholderColor;
  ColorVariant text_color_ = gfx::kPlaceholderColor;
  ColorVariant icon_color_ = gfx::kPlaceholderColor;

  bool enable_background_blur_ = false;
  std::unique_ptr<BlurredBackgroundShield> blurred_background_;

  // Indicates if we are going to use the label contents for tooltip as default.
  bool use_label_as_default_tooltip_ = true;

  // Called to update background color when the button is enabled/disabled.
  base::CallbackListSubscription enabled_changed_subscription_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PillButton, views::LabelButton)
VIEW_BUILDER_PROPERTY(const SkColor, BackgroundColor)
VIEW_BUILDER_PROPERTY(ui::ColorId, BackgroundColorId)
VIEW_BUILDER_PROPERTY(const SkColor, ButtonTextColor)
VIEW_BUILDER_PROPERTY(ui::ColorId, ButtonTextColorId)
VIEW_BUILDER_PROPERTY(const SkColor, IconColor)
VIEW_BUILDER_PROPERTY(ui::ColorId, IconColorId)
VIEW_BUILDER_PROPERTY(PillButton::Type, PillButtonType)
VIEW_BUILDER_PROPERTY(bool, EnableBackgroundBlur)
VIEW_BUILDER_PROPERTY(int, TextWithStringId)
VIEW_BUILDER_PROPERTY(bool, UseLabelAsDefaultTooltip)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PillButton)

#endif  // ASH_STYLE_PILL_BUTTON_H_
