// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_category_type.h"

#include "ash/public/cpp/picker/picker_category.h"

namespace ash {

ASH_EXPORT PickerCategoryType GetPickerCategoryType(PickerCategory category) {
  switch (category) {
    case PickerCategory::kEditorWrite:
      return PickerCategoryType::kEditorWrite;
    case PickerCategory::kEditorRewrite:
      return PickerCategoryType::kEditorRewrite;
    case PickerCategory::kLinks:
    case PickerCategory::kExpressions:
    case PickerCategory::kClipboard:
    case PickerCategory::kDriveFiles:
    case PickerCategory::kLocalFiles:
      return PickerCategoryType::kGeneral;
    case PickerCategory::kDatesTimes:
    case PickerCategory::kUnitsMaths:
      return PickerCategoryType::kCalculations;
    case PickerCategory::kUpperCase:
    case PickerCategory::kLowerCase:
    case PickerCategory::kSentenceCase:
    case PickerCategory::kTitleCase:
      return PickerCategoryType::kCaseTransformations;
    case PickerCategory::kCapsOn:
    case PickerCategory::kCapsOff:
      return PickerCategoryType::kFormatting;
  }
}

}  // namespace ash
