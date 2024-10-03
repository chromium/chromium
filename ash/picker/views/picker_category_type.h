// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_CATEGORY_TYPE_H_
#define ASH_PICKER_VIEWS_PICKER_CATEGORY_TYPE_H_

#include "ash/ash_export.h"
#include "ash/picker/picker_category.h"

namespace ash {

// Used to group related categories together.
enum class ASH_EXPORT PickerCategoryType {
  kNone,
  kEditorWrite,
  kEditorRewrite,
  kLobster,
  kGeneral,
  kMore,
  kCaseTransformations,
};

ASH_EXPORT PickerCategoryType GetPickerCategoryType(PickerCategory category);

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_CATEGORY_TYPE_H_
