// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_STRINGS_H_
#define ASH_PICKER_VIEWS_PICKER_STRINGS_H_

#include <string>

#include "ash/picker/model/picker_category.h"
#include "ash/picker/views/picker_category_type.h"

namespace ash {

std::u16string GetLabelForPickerCategory(PickerCategory category);

std::u16string GetSearchFieldPlaceholderTextForPickerCategory(
    PickerCategory category);

std::u16string GetSectionTitleForPickerCategoryType(
    PickerCategoryType category_type);

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_STRINGS_H_
