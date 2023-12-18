// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_search_results.h"

namespace ash {

PickerSearchResult::PickerSearchResult(const std::u16string& text)
    : text_(text) {}

PickerSearchResults::Section::Section(
    const std::u16string& heading,
    base::span<const PickerSearchResult> results)
    : heading_(heading), results_(results.begin(), results.end()) {}

PickerSearchResults::Section::Section(const Section& other) = default;

PickerSearchResults::Section& PickerSearchResults::Section::operator=(
    const Section& other) = default;

PickerSearchResults::Section::~Section() = default;

PickerSearchResults::PickerSearchResults(base::span<const Section> sections)
    : sections_(sections.begin(), sections.end()) {}

PickerSearchResults::PickerSearchResults(const PickerSearchResults& other) =
    default;

PickerSearchResults& PickerSearchResults::operator=(
    const PickerSearchResults& other) = default;

PickerSearchResults::~PickerSearchResults() = default;

base::span<const PickerSearchResults::Section> PickerSearchResults::sections()
    const {
  return sections_;
}

}  // namespace ash
