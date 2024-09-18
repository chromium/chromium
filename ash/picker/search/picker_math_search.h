// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_MATH_SEARCH_H_
#define ASH_PICKER_SEARCH_PICKER_MATH_SEARCH_H_

#include <optional>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"

namespace ash {

ASH_EXPORT std::optional<PickerSearchResult> PickerMathSearch(
    std::u16string_view query);

ASH_EXPORT std::vector<PickerSearchResult> PickerMathExamples();

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_MATH_SEARCH_H_
