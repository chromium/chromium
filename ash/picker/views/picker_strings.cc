// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_strings.h"

#include "ash/picker/model/picker_category.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

std::u16string GetLabelForPickerCategory(PickerCategory category) {
  switch (category) {
    case PickerCategory::kEmojis:
      return l10n_util::GetStringUTF16(IDS_PICKER_EMOJIS_CATEGORY_LABEL);
    case PickerCategory::kSymbols:
      return l10n_util::GetStringUTF16(IDS_PICKER_SYMBOLS_CATEGORY_LABEL);
    case PickerCategory::kEmoticons:
      return l10n_util::GetStringUTF16(IDS_PICKER_EMOTICONS_CATEGORY_LABEL);
    case PickerCategory::kGifs:
      return l10n_util::GetStringUTF16(IDS_PICKER_GIFS_CATEGORY_LABEL);
  }
}

std::u16string GetSearchFieldPlaceholderTextForPickerCategory(
    PickerCategory category) {
  switch (category) {
    case PickerCategory::kEmojis:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EMOJIS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kSymbols:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_SYMBOLS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kEmoticons:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EMOTICONS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kGifs:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_GIFS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
  }
}

std::u16string GetSectionTitleForPickerCategoryType(
    PickerCategoryType category_type) {
  switch (category_type) {
    case PickerCategoryType::kExpressions:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EXPRESSIONS_CATEGORY_TYPE_SECTION_TITLE);
  }
}

}  // namespace ash
