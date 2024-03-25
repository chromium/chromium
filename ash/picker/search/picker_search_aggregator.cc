// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_aggregator.h"

#include <iterator>
#include <utility>
#include <vector>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"

namespace ash {

namespace {

PickerSectionType SectionTypeFromSearchSource(PickerSearchSource source) {
  switch (source) {
    case PickerSearchSource::kOmnibox:
      return PickerSectionType::kLinks;
    case PickerSearchSource::kTenor:
      return PickerSectionType::kGifs;
    case PickerSearchSource::kEmoji:
      return PickerSectionType::kExpressions;
    case PickerSearchSource::kDate:
    case PickerSearchSource::kMath:
      return PickerSectionType::kSuggestions;
    case PickerSearchSource::kCategory:
      return PickerSectionType::kCategories;
    case PickerSearchSource::kLocalFile:
      return PickerSectionType::kFiles;
    case PickerSearchSource::kDrive:
      return PickerSectionType::kDriveFiles;
  }
}

}  // namespace

PickerSearchAggregator::PickerSearchAggregator(
    base::TimeDelta burn_in_period,
    PickerViewDelegate::SearchResultsCallback callback) {
  current_callback_ = std::move(callback);

  // TODO: b/324154537 - Show a loading animation while waiting for results.
  burn_in_timer_.Start(FROM_HERE, burn_in_period, this,
                       &PickerSearchAggregator::PublishBurnInResults);
}

PickerSearchAggregator::~PickerSearchAggregator() = default;

bool PickerSearchAggregator::IsPostBurnIn() const {
  return !burn_in_timer_.IsRunning();
}

void PickerSearchAggregator::PublishBurnInResults() {
  std::vector<PickerSearchResultsSection> sections;
  if (!suggested_results_.empty()) {
    sections.emplace_back(PickerSectionType::kSuggestions,
                          std::move(suggested_results_));
  }
  if (!category_results_.empty()) {
    sections.emplace_back(PickerSectionType::kCategories,
                          std::move(category_results_));
  }
  if (!emoji_results_.empty()) {
    sections.emplace_back(PickerSectionType::kExpressions,
                          std::move(emoji_results_));
  }
  if (!omnibox_results_.empty()) {
    sections.emplace_back(PickerSectionType::kLinks,
                          std::move(omnibox_results_));
  }
  if (!local_file_results_.empty()) {
    sections.emplace_back(PickerSectionType::kFiles,
                          std::move(local_file_results_));
  }
  if (!drive_file_results_.empty()) {
    sections.emplace_back(PickerSectionType::kDriveFiles,
                          std::move(drive_file_results_));
  }
  if (!gif_results_.empty()) {
    sections.emplace_back(PickerSectionType::kGifs, std::move(gif_results_));
  }
  current_callback_.Run(std::move(sections));
}

void PickerSearchAggregator::HandleSearchSourceResults(
    PickerSearchSource source,
    std::vector<PickerSearchResult> results) {
  // Suggested results have multiple sources, which we store in any order and
  // explicitly do not append if post-burn-in.
  if (source == PickerSearchSource::kDate ||
      source == PickerSearchSource::kMath) {
    base::ranges::move(results, std::back_inserter(suggested_results_));
    return;
  }

  if (IsPostBurnIn()) {
    // Publish post-burn-in results and skip assignment.
    if (!results.empty()) {
      std::vector<PickerSearchResultsSection> sections;
      sections.emplace_back(SectionTypeFromSearchSource(source),
                            std::move(results));
      current_callback_.Run(std::move(sections));
    }
    return;
  }

  switch (source) {
    case PickerSearchSource::kDate:
    case PickerSearchSource::kMath:
      // These should be caught by the above "move into suggested results"
      // if block.
      NOTREACHED() << "Tried assigning suggested results";
      break;
    case PickerSearchSource::kOmnibox:
      omnibox_results_ = std::move(results);
      break;
    case PickerSearchSource::kTenor:
      gif_results_ = std::move(results);
      break;
    case PickerSearchSource::kEmoji:
      emoji_results_ = std::move(results);
      break;
    case PickerSearchSource::kCategory:
      category_results_ = std::move(results);
      break;
    case PickerSearchSource::kLocalFile:
      local_file_results_ = std::move(results);
      break;
    case PickerSearchSource::kDrive:
      drive_file_results_ = std::move(results);
      break;
  }
}

base::WeakPtr<PickerSearchAggregator> PickerSearchAggregator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
