// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_STRINGS_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_STRINGS_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/views/quick_insert_category_type.h"

namespace ash {

std::u16string ASH_EXPORT
GetLabelForQuickInsertCategory(QuickInsertCategory category);

std::u16string ASH_EXPORT GetSearchFieldPlaceholderTextForQuickInsertCategory(
    QuickInsertCategory category);

std::u16string ASH_EXPORT GetSectionTitleForQuickInsertCategoryType(
    QuickInsertCategoryType category_type);

std::u16string ASH_EXPORT
GetSectionTitleForQuickInsertSectionType(QuickInsertSectionType section_type);

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_STRINGS_H_
