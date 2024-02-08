// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_category_type.h"

#include "ash/picker/model/picker_category.h"

namespace ash {

ASH_EXPORT PickerCategoryType GetPickerCategoryType(PickerCategory category) {
  switch (category) {
    case PickerCategory::kEmojis:
    case PickerCategory::kSymbols:
    case PickerCategory::kEmoticons:
    case PickerCategory::kGifs:
      return PickerCategoryType::kExpressions;
    case PickerCategory::kOpenTabs:
    case PickerCategory::kBrowsingHistory:
    case PickerCategory::kBookmarks:
      return PickerCategoryType::kLinks;
  }
}

}  // namespace ash
