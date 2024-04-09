// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_search_results_section.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/containers/span.h"

namespace ash {

PickerSearchResultsSection::PickerSearchResultsSection(
    PickerSectionType type,
    std::vector<PickerSearchResult> results)
    : type_(type), results_(std::move(results)) {}

PickerSearchResultsSection::PickerSearchResultsSection(
    const PickerSearchResultsSection& other) = default;

PickerSearchResultsSection& PickerSearchResultsSection::operator=(
    const PickerSearchResultsSection& other) = default;

PickerSearchResultsSection::PickerSearchResultsSection(
    PickerSearchResultsSection&& other) = default;

PickerSearchResultsSection& PickerSearchResultsSection::operator=(
    PickerSearchResultsSection&& other) = default;

PickerSearchResultsSection::~PickerSearchResultsSection() = default;

PickerSectionType PickerSearchResultsSection::type() const {
  return type_;
}

base::span<const PickerSearchResult> PickerSearchResultsSection::results()
    const {
  return results_;
}

bool PickerSearchResultsSection::has_more_results() const {
  // TODO: b/322081736 - Determine whether there are more results by passing an
  // argument to `PickerSearchResultsSection`.
  switch (type_) {
    case PickerSectionType::kCategories:
    case PickerSectionType::kSuggestions:
    case PickerSectionType::kRecentlyUsed:
      return false;
    case PickerSectionType::kExpressions:
    case PickerSectionType::kLinks:
    case PickerSectionType::kFiles:
    case PickerSectionType::kDriveFiles:
    case PickerSectionType::kGifs:
      return true;
  }
}

}  // namespace ash
