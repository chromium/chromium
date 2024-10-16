// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_DATE_SEARCH_H_
#define ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_DATE_SEARCH_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_search_result.h"

namespace base {
class Time;
}

namespace ash {

ASH_EXPORT std::vector<QuickInsertSearchResult> PickerDateSearch(
    const base::Time& now,
    std::u16string_view query);

ASH_EXPORT std::vector<QuickInsertSearchResult> PickerSuggestedDateResults();

}  // namespace ash

#endif  // ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_DATE_SEARCH_H_
