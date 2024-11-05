// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_ACTION_SEARCH_H_
#define ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_ACTION_SEARCH_H_

#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/containers/span.h"

namespace ash {

enum class QuickInsertCategory;

ASH_EXPORT std::vector<QuickInsertSearchResult> PickerActionSearch(
    base::span<const QuickInsertCategory> available_categories,
    bool caps_lock_state_to_search,
    bool search_case_transforms,
    std::u16string_view query);

}  // namespace ash

#endif  // ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_ACTION_SEARCH_H_
