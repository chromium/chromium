// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_ACTION_SEARCH_H_
#define ASH_PICKER_SEARCH_PICKER_ACTION_SEARCH_H_

#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"
#include "base/containers/span.h"

namespace ash {

enum class PickerCategory;

struct PickerActionSearchOptions {
  base::span<const PickerCategory> available_categories;
  bool caps_lock_state_to_search = false;
  bool search_case_transforms = false;
};

ASH_EXPORT std::vector<PickerSearchResult> PickerActionSearch(
    const PickerActionSearchOptions& options,
    std::u16string_view query);

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_ACTION_SEARCH_H_
