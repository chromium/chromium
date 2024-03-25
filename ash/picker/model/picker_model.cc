// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_model.h"

#include "ash/public/cpp/picker/picker_category.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/range/range.h"

namespace ash {
namespace {

bool HasSelectedText(ui::TextInputClient* client) {
  gfx::Range selection_range;
  if (client == nullptr ||
      !client->GetEditableSelectionRange(&selection_range)) {
    return false;
  }

  return selection_range.IsValid() && !selection_range.is_empty();
}

}  // namespace

PickerModel::PickerModel(ui::TextInputClient* focused_client)
    : has_selected_text_(HasSelectedText(focused_client)) {}

std::vector<PickerCategory> PickerModel::GetAvailableCategories() const {
  if (has_selected_text_) {
    return std::vector<PickerCategory>{
        PickerCategory::kEditor,
    };
  }

  return std::vector<PickerCategory>{
      PickerCategory::kEmojis,     PickerCategory::kSymbols,
      PickerCategory::kEmoticons,  PickerCategory::kGifs,
      PickerCategory::kOpenTabs,   PickerCategory::kBrowsingHistory,
      PickerCategory::kBookmarks,  PickerCategory::kDriveFiles,
      PickerCategory::kLocalFiles, PickerCategory::kEditor,
      PickerCategory::kDatesTimes, PickerCategory::kUnitsMaths,
  };
}

}  // namespace ash
