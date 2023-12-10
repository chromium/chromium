// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_nudge_label.h"

#include <memory>

#include "ash/style/ash_color_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

SystemNudgeLabel::SystemNudgeLabel(std::u16string text, int fixed_width)
    : styled_label_(AddChildView(std::make_unique<views::StyledLabel>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>());
  styled_label_->SetDisplayedOnBackgroundColor(SK_ColorTRANSPARENT);
  styled_label_->SetText(std::move(text));
  styled_label_->SizeToFit(fixed_width);
}

SystemNudgeLabel::~SystemNudgeLabel() = default;

void SystemNudgeLabel::AddCustomView(std::unique_ptr<View> custom_view,
                                     size_t offset) {
  DCHECK_LT(offset, styled_label_->GetText().size());

  views::StyledLabel::RangeStyleInfo custom_view_style;
  custom_view_style.custom_view = custom_view.get();

  custom_view_styles_by_offset_.insert_or_assign(offset,
                                                 std::move(custom_view_style));
  styled_label_->AddCustomView(std::move(custom_view));
}

const std::u16string& SystemNudgeLabel::GetText() const {
  return styled_label_->GetText();
}

void SystemNudgeLabel::OnThemeChanged() {
  views::View::OnThemeChanged();
  views::StyledLabel::RangeStyleInfo text_style;
  text_style.override_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  text_style.custom_font =
      styled_label_->GetFontList().DeriveWithSizeDelta(font_size_delta_);
  styled_label_->ClearStyleRanges();

  size_t i = 0;
  for (const auto& [offset, custom_view_style] :
       custom_view_styles_by_offset_) {
    if (i < offset) {
      // Add a style range for text that precedes a custom view.
      styled_label_->AddStyleRange(gfx::Range(i, offset), text_style);
    }
    i = offset + 1;

    // Add a style range for a custom view within the text.
    styled_label_->AddStyleRange(gfx::Range(offset, offset + 1),
                                 custom_view_style);
  }

  // Add a style range for any text that follows the last custom view.
  size_t text_length = styled_label_->GetText().length();
  if (i < text_length) {
    styled_label_->AddStyleRange(gfx::Range(i, text_length), text_style);
  }
}

BEGIN_METADATA(SystemNudgeLabel)
END_METADATA

}  // namespace ash
