// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_MATH_SEARCH_H_
#define ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_MATH_SEARCH_H_

#include <optional>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_search_result.h"

namespace ash {

ASH_EXPORT std::optional<QuickInsertSearchResult> PickerMathSearch(
    std::u16string_view query);

ASH_EXPORT std::vector<QuickInsertSearchResult> PickerMathExamples();

}  // namespace ash

#endif  // ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_MATH_SEARCH_H_
