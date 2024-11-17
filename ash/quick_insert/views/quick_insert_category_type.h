// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_CATEGORY_TYPE_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_CATEGORY_TYPE_H_

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_category.h"

namespace ash {

// Used to group related categories together.
enum class ASH_EXPORT QuickInsertCategoryType {
  kNone,
  kEditorWrite,
  kEditorRewrite,
  kLobster,
  kGeneral,
  kMore,
  kCaseTransformations,
};

ASH_EXPORT QuickInsertCategoryType
GetQuickInsertCategoryType(QuickInsertCategory category);

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_CATEGORY_TYPE_H_
