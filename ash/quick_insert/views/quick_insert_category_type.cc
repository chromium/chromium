// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_category_type.h"

#include "ash/quick_insert/quick_insert_category.h"

namespace ash {

ASH_EXPORT QuickInsertCategoryType
GetQuickInsertCategoryType(QuickInsertCategory category) {
  switch (category) {
    case QuickInsertCategory::kEditorWrite:
      return QuickInsertCategoryType::kEditorWrite;
    case QuickInsertCategory::kEditorRewrite:
      return QuickInsertCategoryType::kEditorRewrite;
    case QuickInsertCategory::kLobsterWithNoSelectedText:
      return QuickInsertCategoryType::kEditorWrite;
    case QuickInsertCategory::kLobsterWithSelectedText:
      return QuickInsertCategoryType::kEditorRewrite;
    case QuickInsertCategory::kLinks:
    case QuickInsertCategory::kEmojisGifs:
    case QuickInsertCategory::kEmojis:
    case QuickInsertCategory::kClipboard:
    case QuickInsertCategory::kDriveFiles:
    case QuickInsertCategory::kLocalFiles:
      return QuickInsertCategoryType::kGeneral;
    case QuickInsertCategory::kDatesTimes:
    case QuickInsertCategory::kUnitsMaths:
      return QuickInsertCategoryType::kMore;
  }
}

}  // namespace ash
