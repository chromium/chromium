// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_ui.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::holding_space_ui {

views::Builder<views::Label> CreateTopLevelBubbleHeaderLabel(int message_id) {
  return views::Builder<views::Label>(
      bubble_utils::CreateLabel(bubble_utils::LabelStyle::kHeader,
                                l10n_util::GetStringUTF16(message_id)));
}

views::Builder<views::Label> CreateSectionHeaderLabel(int message_id) {
  bubble_utils::LabelStyle style = bubble_utils::LabelStyle::kHeader;
  bubble_utils::LabelStyleOverrides overrides;

  if (features::IsHoldingSpaceRefreshEnabled()) {
    style = bubble_utils::LabelStyle::kBody;
    overrides.font_weight = gfx::Font::Weight::MEDIUM;
  }

  return views::Builder<views::Label>(bubble_utils::CreateLabel(
      style, l10n_util::GetStringUTF16(message_id), overrides));
}

views::Builder<views::Label> CreateSuggestionsSectionHeaderLabel(
    int message_id) {
  return views::Builder<views::Label>(bubble_utils::CreateLabel(
      bubble_utils::LabelStyle::kSubheader,
      l10n_util::GetStringUTF16(message_id),
      bubble_utils::LabelStyleOverrides(
          /*font_weight=*/absl::nullopt, /*text_color=*/absl::nullopt)));
}

views::Builder<views::Label> CreateBubblePlaceholderLabel(int message_id) {
  return views::Builder<views::Label>(bubble_utils::CreateLabel(
      bubble_utils::LabelStyle::kHeader, l10n_util::GetStringUTF16(message_id),
      bubble_utils::LabelStyleOverrides(
          /*font_weight=*/absl::nullopt, /*text_color=*/
          AshColorProvider::ContentLayerType::kTextColorSecondary)));
}

views::Builder<views::Label> CreateSectionPlaceholderLabel(
    const std::u16string& text) {
  bubble_utils::LabelStyleOverrides overrides;

  if (features::IsHoldingSpaceSuggestionsEnabled()) {
    overrides.text_color =
        AshColorProvider::ContentLayerType::kTextColorSecondary;
  }

  return views::Builder<views::Label>(bubble_utils::CreateLabel(
      bubble_utils::LabelStyle::kBody, text, overrides));
}

}  // namespace ash::holding_space_ui
