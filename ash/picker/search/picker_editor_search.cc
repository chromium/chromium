// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_editor_search.h"

#include <optional>
#include <string>
#include <string_view>

#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash {
namespace {

constexpr int kMinWordsNeededForEditorMatch = 4;

}

std::optional<PickerSearchResult> PickerEditorSearch(
    PickerSearchResult::EditorData::Mode mode,
    std::u16string_view query) {
  CHECK(!query.empty());
  string_matching::TokenizedString tokenized_query{std::u16string(query)};
  return tokenized_query.tokens().size() >= kMinWordsNeededForEditorMatch
             ? std::make_optional(PickerSearchResult::Editor(
                   mode, /*display_name=*/u"", /*category=*/std::nullopt,
                   /*preset_query_id=*/std::nullopt))
             : std::nullopt;
}

}  // namespace ash
