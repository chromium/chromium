// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_OPTION_BUTTON_BASE_H_
#define ASH_STYLE_OPTION_BUTTON_BASE_H_

#include "ash/ash_export.h"
#include "ash/style/typography.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/button/label_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class ASH_EXPORT OptionButtonBase : public views::LabelButton {
  METADATA_HEADER(OptionButtonBase, views::LabelButton)

 public:
  enum class ClickBehavior {
    kNone,
    kSetToOn,
    kToggle,
  };

  using ButtonSelectedCallback =
      base::RepeatingCallback<void(OptionButtonBase* button)>;

  // The default padding for the button if the client doesn't explicitly set
  // one.
  static constexpr auto kDefaultPadding = gfx::Insets::TLBR(8, 12, 8, 12);

  static constexpr int kIconSize = 20;

  static constexpr int kImageLabelSpacingDP = 12;

  OptionButtonBase(int button_width,
                   PressedCallback callback,
                   const std::u16string& label = std::u16string(),
                   const gfx::Insets& insets = kDefaultPadding,
                   int image_label_spacing = kImageLabelSpacingDP,
                   ClickBehavior click_behavior = ClickBehavior::kNone);
  OptionButtonBase(const OptionButtonBase&) = delete;
  OptionButtonBase& operator=(const OptionButtonBase&) = delete;
  ~OptionButtonBase() override;

  bool selected() const { return selected_; }
  void set_button_selected_callback(ButtonSelectedCallback callback) {
    button_selected_callback_ = std::move(callback);
  }

  // Updates the `select_` state.
  void SetSelected(bool selected);
  virtual void OnSelectedChanged() {}

  // Sets a TypographyToken as the style of the label.
  void SetLabelStyle(TypographyToken token);
  // Sets a color_id as the color_id of the label.
  void SetLabelColorId(ui::ColorId color_id);
  void SetLabelFontList(const gfx::FontList& font_list);

  // views::LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  void Layout(PassKey) override;
  void OnThemeChanged() override;
  void NotifyClick(const ui::Event& event) override;

 protected:
  // `icon_state` is a bitmask using the IconState enum.
  SkColor GetIconImageColor() const;

  // Returns the vector icon according to the button's type and selected state.
  virtual const gfx::VectorIcon& GetVectorIcon() const = 0;

  // Returns true if the icon is on the button's left side.
  virtual bool IsIconOnTheLeftSide() = 0;

 private:
  // Update the label's color based on the enable state.
  void UpdateTextColor();

  void SetAndUpdateAccessibleDefaultActionVerb();

  const int min_width_;

  const int image_label_spacing_;

  // True if the button is currently selected.
  bool selected_ = false;

  const ClickBehavior click_behavior_;

  ButtonSelectedCallback button_selected_callback_;
};

}  // namespace ash

#endif  // ASH_STYLE_OPTION_BUTTON_BASE_H_
