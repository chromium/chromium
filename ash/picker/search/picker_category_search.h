// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_CATEGORY_SEARCH_H_
#define ASH_PICKER_SEARCH_PICKER_CATEGORY_SEARCH_H_

#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/span.h"

namespace ash {

class PickerSearchResult;
enum class PickerCategory;

ASH_EXPORT std::vector<PickerSearchResult> PickerCategorySearch(
    base::span<const PickerCategory> categories,
    std::u16string_view query);

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_CATEGORY_SEARCH_H_
