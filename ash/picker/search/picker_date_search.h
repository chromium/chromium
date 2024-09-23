// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_DATE_SEARCH_H_
#define ASH_PICKER_SEARCH_PICKER_DATE_SEARCH_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"

namespace base {
class Time;
}

namespace ash {

ASH_EXPORT std::vector<PickerSearchResult> PickerDateSearch(
    const base::Time& now,
    std::u16string_view query);

ASH_EXPORT std::vector<PickerSearchResult> PickerSuggestedDateResults();

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_DATE_SEARCH_H_
