// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_STRINGS_H_
#define ASH_PICKER_VIEWS_PICKER_STRINGS_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/views/picker_category_type.h"

namespace ash {

std::u16string ASH_EXPORT GetLabelForPickerCategory(PickerCategory category);

std::u16string ASH_EXPORT
GetSearchFieldPlaceholderTextForPickerCategory(PickerCategory category);

std::u16string ASH_EXPORT
GetSectionTitleForPickerCategoryType(PickerCategoryType category_type);

std::u16string ASH_EXPORT
GetSectionTitleForPickerSectionType(PickerSectionType section_type);

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_STRINGS_H_
