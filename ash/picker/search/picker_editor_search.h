// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_EDITOR_SEARCH_H_
#define ASH_PICKER_SEARCH_PICKER_EDITOR_SEARCH_H_

#include <optional>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/picker/picker_search_result.h"

namespace ash {

// `query` must not be empty.
ASH_EXPORT std::optional<PickerSearchResult> PickerEditorSearch(
    PickerEditorResult::Mode mode,
    std::u16string_view query);

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_EDITOR_SEARCH_H_
