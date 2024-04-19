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
    case PickerSearchSource::kClipboard:
      return PickerSectionType::kSuggestions;
    case PickerSearchSource::kCategory:
      return PickerSectionType::kCategories;
    case PickerSearchSource::kLocalFile:
      return PickerSectionType::kFiles;
    case PickerSearchSource::kDrive:
      return PickerSectionType::kDriveFiles;
    case PickerSearchSource::kEditorWrite:
      return PickerSectionType::kEditorWrite;
    case PickerSearchSource::kEditorRewrite:
      return PickerSectionType::kEditorRewrite;
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

void PickerSearchAggregator::HandleSearchSourceResults(
    PickerSearchSource source,
    std::vector<PickerSearchResult> results,
    bool has_more_results) {
  // GIF results must appear later than Drive results. In the case where GIF
  // search finishes before Drive search, store the GIF results for when Drive
  // search finishes.
  if (source == PickerSearchSource::kTenor && !drive_search_finished_) {
    pending_gif_results_ = std::move(results);
    return;
  }

  HandleSearchSourceResultsImpl(source, std::move(results), has_more_results);

  if (source == PickerSearchSource::kDrive) {
    drive_search_finished_ = true;
    if (pending_gif_results_.has_value()) {
      HandleSearchSourceResultsImpl(PickerSearchSource::kTenor,
                                    std::move(*pending_gif_results_),
                                    /*has_more_results=*/true);
      pending_gif_results_ = std::nullopt;
    }
  }
}

PickerSearchAggregator::PickerSearchResults::PickerSearchResults() = default;

PickerSearchAggregator::PickerSearchResults::PickerSearchResults(
    std::vector<PickerSearchResult> results,
    bool has_more)
    : results(std::move(results)), has_more(has_more) {}

PickerSearchAggregator::PickerSearchResults::PickerSearchResults(
    PickerSearchResults&& other) = default;

PickerSearchAggregator::PickerSearchResults&
PickerSearchAggregator::PickerSearchResults::operator=(
    PickerSearchResults&& other) = default;

PickerSearchAggregator::PickerSearchResults::~PickerSearchResults() = default;

bool PickerSearchAggregator::IsPostBurnIn() const {
  return !burn_in_timer_.IsRunning();
}

void PickerSearchAggregator::PublishBurnInResults() {
  std::vector<PickerSearchResultsSection> sections;
  for (PickerSectionType type : {
           PickerSectionType::kSuggestions,
           PickerSectionType::kCategories,
           PickerSectionType::kEditorWrite,
           PickerSectionType::kEditorRewrite,
           PickerSectionType::kExpressions,
           PickerSectionType::kLinks,
           PickerSectionType::kFiles,
           PickerSectionType::kDriveFiles,
           PickerSectionType::kGifs,
       }) {
    if (auto it = results_.find(type);
        it != results_.end() && !it->second.results.empty()) {
      sections.emplace_back(type, std::move(it->second.results),
                            it->second.has_more);
    }
  }
  current_callback_.Run(std::move(sections));
}

void PickerSearchAggregator::HandleSearchSourceResultsImpl(
    PickerSearchSource source,
    std::vector<PickerSearchResult> results,
    bool has_more_results) {
  // Suggested results have multiple sources, which we store in any order and
  // explicitly do not append if post-burn-in.
  if (source == PickerSearchSource::kDate ||
      source == PickerSearchSource::kMath ||
      source == PickerSearchSource::kClipboard) {
    // Suggested results cannot have more results, since it's not a proper
    // category.
    CHECK(!has_more_results);
    base::ranges::move(
        results,
        std::back_inserter(results_[PickerSectionType::kSuggestions].results));
    return;
  }

  if (IsPostBurnIn()) {
    // Publish post-burn-in results and skip assignment.
    if (!results.empty()) {
      std::vector<PickerSearchResultsSection> sections;
      sections.emplace_back(SectionTypeFromSearchSource(source),
                            std::move(results), has_more_results);
      current_callback_.Run(std::move(sections));
    }
    return;
  }

  const auto& [unused, inserted] = results_.emplace(
      SectionTypeFromSearchSource(source),
      PickerSearchResults(std::move(results), has_more_results));
  CHECK(inserted);
}

base::WeakPtr<PickerSearchAggregator> PickerSearchAggregator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
