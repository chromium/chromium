// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_date_search.h"

#include <optional>
#include <string>

#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"

namespace ash {

std::optional<PickerSearchResult> PickerDateSearch(const base::Time& now,
                                                   std::u16string_view query) {
  if (query == u"today") {
    return PickerSearchResult::Text(
        base::LocalizedTimeFormatWithPattern(now, "LLLd"));
  }
  return std::nullopt;
}

}  // namespace ash
