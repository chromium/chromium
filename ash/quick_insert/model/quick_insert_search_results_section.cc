// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/model/quick_insert_search_results_section.h"

#include <utility>
#include <vector>

#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/containers/span.h"

namespace ash {

QuickInsertSearchResultsSection::QuickInsertSearchResultsSection(
    QuickInsertSectionType type,
    std::vector<QuickInsertSearchResult> results,
    bool has_more_results)
    : type_(type),
      results_(std::move(results)),
      has_more_results_(has_more_results) {}

QuickInsertSearchResultsSection::QuickInsertSearchResultsSection(
    const QuickInsertSearchResultsSection& other) = default;

QuickInsertSearchResultsSection& QuickInsertSearchResultsSection::operator=(
    const QuickInsertSearchResultsSection& other) = default;

QuickInsertSearchResultsSection::QuickInsertSearchResultsSection(
    QuickInsertSearchResultsSection&& other) = default;

QuickInsertSearchResultsSection& QuickInsertSearchResultsSection::operator=(
    QuickInsertSearchResultsSection&& other) = default;

QuickInsertSearchResultsSection::~QuickInsertSearchResultsSection() = default;

QuickInsertSectionType QuickInsertSearchResultsSection::type() const {
  return type_;
}

base::span<const QuickInsertSearchResult>
QuickInsertSearchResultsSection::results() const {
  return results_;
}

bool QuickInsertSearchResultsSection::has_more_results() const {
  return has_more_results_;
}

}  // namespace ash
