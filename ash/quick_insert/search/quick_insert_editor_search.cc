// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_editor_search.h"

#include <optional>
#include <string>
#include <string_view>

#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/check.h"
#include "base/i18n/char_iterator.h"

namespace ash {
namespace {

// An Editor match requires at least 3 characters.
bool HasMinCharsNeededForEditorMatch(std::u16string_view query) {
  base::i18n::UTF16CharIterator it(query);
  return it.Advance() && it.Advance() && it.Advance();
}
}  // namespace

std::optional<QuickInsertSearchResult> PickerEditorSearch(
    QuickInsertEditorResult::Mode mode,
    std::u16string_view query) {
  CHECK(!query.empty());
  return HasMinCharsNeededForEditorMatch(query)
             ? std::make_optional(QuickInsertEditorResult(
                   mode, /*display_name=*/u"", /*category=*/std::nullopt,
                   /*preset_query_id=*/std::nullopt))
             : std::nullopt;
}

}  // namespace ash
