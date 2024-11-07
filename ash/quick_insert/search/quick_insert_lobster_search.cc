// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>

#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/search/quick_insert_editor_search.h"
#include "base/check.h"
#include "base/i18n/char_iterator.h"

namespace ash {

std::optional<QuickInsertSearchResult> PickerLobsterSearch(
    QuickInsertLobsterResult::Mode mode,
    std::u16string_view query) {
  CHECK(!query.empty());
  // TODO: b/369508495 - implement matching logic.
  return std::make_optional(
      QuickInsertLobsterResult(mode, /*display_name=*/u""));
}

}  // namespace ash
