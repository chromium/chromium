// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_search_results.h"

#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/containers/span.h"

namespace ash {

PickerSearchResults::Section::Section(
    PickerSectionType type,
    base::span<const PickerSearchResult> results)
    : type_(type), results_(results.begin(), results.end()) {}

PickerSearchResults::Section::Section(const Section& other) = default;

PickerSearchResults::Section& PickerSearchResults::Section::operator=(
    const Section& other) = default;

PickerSearchResults::Section::~Section() = default;

PickerSectionType PickerSearchResults::Section::type() const {
  return type_;
}

base::span<const PickerSearchResult> PickerSearchResults::Section::results()
    const {
  return results_;
}

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
