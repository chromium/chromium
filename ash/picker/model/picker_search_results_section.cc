// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_search_results_section.h"

#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/containers/span.h"

namespace ash {

PickerSearchResultsSection::PickerSearchResultsSection(
    PickerSectionType type,
    base::span<const PickerSearchResult> results)
    : type_(type), results_(results.begin(), results.end()) {}

PickerSearchResultsSection::PickerSearchResultsSection(
    const PickerSearchResultsSection& other) = default;

PickerSearchResultsSection& PickerSearchResultsSection::operator=(
    const PickerSearchResultsSection& other) = default;

PickerSearchResultsSection::~PickerSearchResultsSection() = default;

PickerSectionType PickerSearchResultsSection::type() const {
  return type_;
}

base::span<const PickerSearchResult> PickerSearchResultsSection::results()
    const {
  return results_;
}

}  // namespace ash
