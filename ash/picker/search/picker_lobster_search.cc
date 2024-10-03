// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>

#include "ash/picker/picker_search_result.h"
#include "ash/picker/search/picker_editor_search.h"
#include "base/check.h"
#include "base/i18n/char_iterator.h"

namespace ash {

std::optional<PickerSearchResult> PickerLobsterSearch(
    std::u16string_view query) {
  CHECK(!query.empty());
  // TODO: b/369508495 - implement matching logic.
  return std::make_optional(PickerLobsterResult(/*display_name=*/u""));
}

}  // namespace ash
