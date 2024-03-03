// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_model.h"

#include "ash/public/cpp/picker/picker_category.h"

namespace ash {

std::vector<PickerCategory> PickerModel::GetAvailableCategories() const {
  return std::vector<PickerCategory>{
      PickerCategory::kEmojis,     PickerCategory::kSymbols,
      PickerCategory::kEmoticons,  PickerCategory::kGifs,
      PickerCategory::kOpenTabs,   PickerCategory::kBrowsingHistory,
      PickerCategory::kBookmarks,  PickerCategory::kDriveFiles,
      PickerCategory::kLocalFiles,
  };
}

}  // namespace ash
