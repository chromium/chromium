// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_model.h"

#include "ash/public/cpp/picker/picker_category.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/range/range.h"

namespace ash {
namespace {

std::u16string GetSelectedText(ui::TextInputClient* client) {
  gfx::Range selection_range;
  std::u16string result;
  if (client && client->GetEditableSelectionRange(&selection_range) &&
      selection_range.IsValid() && !selection_range.is_empty() &&
      client->GetTextFromRange(selection_range, &result)) {
    return result;
  }
  return u"";
}

}  // namespace

PickerModel::PickerModel(ui::TextInputClient* focused_client)
    : selected_text_(GetSelectedText(focused_client)) {}

std::vector<PickerCategory> PickerModel::GetAvailableCategories() const {
  if (HasSelectedText()) {
    return std::vector<PickerCategory>{
        PickerCategory::kUpperCase,
        PickerCategory::kLowerCase,
        PickerCategory::kSentenceCase,
        PickerCategory::kTitleCase,
    };
  }

  return std::vector<PickerCategory>{
      PickerCategory::kLinks,      PickerCategory::kExpressions,
      PickerCategory::kClipboard,  PickerCategory::kDriveFiles,
      PickerCategory::kLocalFiles, PickerCategory::kDatesTimes,
      PickerCategory::kUnitsMaths,
  };
}

bool PickerModel::HasSelectedText() const {
  return !selected_text_.empty();
}

std::u16string_view PickerModel::selected_text() const {
  return selected_text_;
}

}  // namespace ash
