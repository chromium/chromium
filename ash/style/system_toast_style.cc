// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_toast_style.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/highlight_border.h"
#include "ash/style/pill_button.h"
#include "ash/system/toast/toast_overlay.h"
#include "ash/wm/work_area_insets.h"
#include "base/strings/strcat.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// UI constants in DIP (Density Independent Pixel).
constexpr int kToastTextMaximumWidth = 512;
constexpr int kOneLineHorizontalSpacing = 16;
constexpr int kTwoLineHorizontalSpacing = 24;
constexpr int kSpacingBetweenLabelAndButton = 16;
constexpr int kOnelineButtonRightSpacing = 2;
constexpr int kTwolineButtonRightSpacing = 12;
constexpr int kToastLabelVerticalSpacing = 8;
constexpr int kManagedIconSize = 32;

// The label inside SystemToastStyle, which allows two lines at maximum.
class SystemToastInnerLabel : public views::Label {
 public:
  METADATA_HEADER(SystemToastInnerLabel);
  explicit SystemToastInnerLabel(const std::u16string& text)
      : views::Label(text) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetAutoColorReadabilityEnabled(false);
    SetMultiLine(true);
    SetMaximumWidth(kToastTextMaximumWidth);
    SetMaxLines(2);
    SetSubpixelRenderingEnabled(false);

    SetFontList(views::Label::GetDefaultFontList().Derive(
        2, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  }

  SystemToastInnerLabel(const SystemToastInnerLabel&) = delete;
  SystemToastInnerLabel& operator=(const SystemToastInnerLabel&) = delete;
  ~SystemToastInnerLabel() override = default;

 private:
  // views::Label:
  void OnThemeChanged() override {
    views::Label::OnThemeChanged();
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
};

BEGIN_METADATA(SystemToastInnerLabel, views::Label)
END_METADATA

SkColor GetBackgroundColor() {
  return AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
}

// TODO(crbug/1294449): Handle the case when a word can't be fitted into
// one-line where additional spaces need to be padded.
bool FormatDisplayLabelText(views::Label* label,
                            std::u16string& out_display_text) {
  const gfx::FontList& font_list = label->font_list();
  const std::u16string& text = label->GetText();
  const int label_text_width = gfx::GetStringWidth(text, font_list);
  out_display_text = text;
  const bool two_line = label_text_width > kToastTextMaximumWidth;
  if (two_line) {
    // Find the index to split string into multiple lines so that the width of
    // each row is within `kToastTextMaximumWidth` limit.
    const int split_index =
        text.length() * kToastTextMaximumWidth / label_text_width;
    out_display_text = base::StrCat(
        {text.substr(0, split_index), u"\n" + text.substr(split_index)});
  }

  out_display_text = gfx::ElideText(
      out_display_text, font_list, kToastTextMaximumWidth * 2, gfx::ELIDE_TAIL);

  return two_line;
}

}  // namespace

SystemToastStyle::SystemToastStyle(
    base::RepeatingClosure dismiss_callback,
    const std::u16string& text,
    const absl::optional<std::u16string>& dismiss_text,
    const bool is_managed) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);

  if (is_managed) {
    managed_icon_ = AddChildView(std::make_unique<views::ImageView>());
    managed_icon_->SetPreferredSize(
        gfx::Size(kManagedIconSize, kManagedIconSize));
  }

  label_ = AddChildView(std::make_unique<SystemToastInnerLabel>(text));

  std::u16string display_text;
  const bool two_line = FormatDisplayLabelText(label_, display_text);
  label_->SetText(display_text);

  if (dismiss_text.has_value()) {
    button_ = AddChildView(std::make_unique<PillButton>(
        std::move(dismiss_callback),
        dismiss_text.value().empty()
            ? l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON)
            : dismiss_text.value(),
        PillButton::Type::kIconlessAccentFloating,
        /*icon=*/nullptr));
    button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }

  int toast_height = kToastLabelVerticalSpacing * 2 + label_->GetLineHeight();
  if (two_line)
    toast_height += label_->GetLineHeight();

  const int vertical_spacing =
      button_
          ? std::min(kToastLabelVerticalSpacing,
                     (toast_height - button_->GetPreferredSize().height()) / 2)
          : kToastLabelVerticalSpacing;

  auto insets =
      gfx::Insets::VH(vertical_spacing, two_line ? kTwoLineHorizontalSpacing
                                                 : kOneLineHorizontalSpacing);
  if (button_) {
    insets.set_right(two_line ? kTwolineButtonRightSpacing
                              : kOnelineButtonRightSpacing);
  }

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, insets,
      button_ ? kSpacingBetweenLabelAndButton : 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->SetFlexForView(label_, 1);

  const int toast_corner_radius = toast_height / 2.f;
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(toast_corner_radius));
  SetBackground(views::CreateRoundedRectBackground(GetBackgroundColor(),
                                                   toast_corner_radius));
  if (features::IsDarkLightModeEnabled()) {
    SetBorder(std::make_unique<HighlightBorder>(
        toast_corner_radius, HighlightBorder::Type::kHighlightBorder1,
        /*use_light_colors=*/false));
  }
}

SystemToastStyle::~SystemToastStyle() = default;

void SystemToastStyle::SetText(const std::u16string& text) {
  label_->SetText(text);
}

void SystemToastStyle::OnThemeChanged() {
  views::View::OnThemeChanged();

  background()->SetNativeControlColor(GetBackgroundColor());

  if (managed_icon_) {
    managed_icon_->SetImage(gfx::CreateVectorIcon(
        kSystemMenuBusinessIcon,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
  }

  SchedulePaint();
}

BEGIN_METADATA(SystemToastStyle, views::View)
END_METADATA

}  // namespace ash
