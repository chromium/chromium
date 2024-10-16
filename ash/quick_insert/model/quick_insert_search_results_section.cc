// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/model/quick_insert_search_results_section.h"

#include <utility>
#include <vector>

#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/containers/span.h"

namespace ash {

PickerSearchResultsSection::PickerSearchResultsSection(
    PickerSectionType type,
    std::vector<QuickInsertSearchResult> results,
    bool has_more_results)
    : type_(type),
      results_(std::move(results)),
      has_more_results_(has_more_results) {}

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

base::span<const QuickInsertSearchResult> PickerSearchResultsSection::results()
    const {
  return results_;
}

bool PickerSearchResultsSection::has_more_results() const {
  return has_more_results_;
}

}  // namespace ash
