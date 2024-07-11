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
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/overloaded.h"
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
    case PickerSearchSource::kDate:
    case PickerSearchSource::kMath:
      return PickerSectionType::kNone;
    case PickerSearchSource::kClipboard:
      return PickerSectionType::kClipboard;
    case PickerSearchSource::kAction:
      return PickerSectionType::kNone;
    case PickerSearchSource::kLocalFile:
      return PickerSectionType::kLocalFiles;
    case PickerSearchSource::kDrive:
      return PickerSectionType::kDriveFiles;
    case PickerSearchSource::kEditorWrite:
      return PickerSectionType::kEditorWrite;
    case PickerSearchSource::kEditorRewrite:
      return PickerSectionType::kEditorRewrite;
  }
}

bool ShouldPromote(const PickerSearchResult& result) {
  return std::visit(
      base::Overloaded{[](const PickerSearchResult::ClipboardData& data) {
                         return data.is_recent;
                       },
                       [](const PickerSearchResult::BrowsingHistoryData& data) {
                         return data.best_match;
                       },
                       [](const PickerSearchResult::LocalFileData& data) {
                         return data.best_match;
                       },
                       [](const PickerSearchResult::DriveFileData& data) {
                         return data.best_match;
                       },
                       [](const auto& data) { return false; }},
      result.data());
}

}  // namespace

PickerSearchAggregator::PickerSearchAggregator(
    base::TimeDelta burn_in_period,
    PickerViewDelegate::SearchResultsCallback callback) {
  current_callback_ = std::move(callback);
  CHECK(!current_callback_.is_null());

  // TODO: b/324154537 - Show a loading animation while waiting for results.
  burn_in_timer_.Start(FROM_HERE, burn_in_period, this,
                       &PickerSearchAggregator::PublishBurnInResults);
}

PickerSearchAggregator::~PickerSearchAggregator() = default;

void PickerSearchAggregator::HandleSearchSourceResults(
    PickerSearchSource source,
    std::vector<PickerSearchResult> results,
    bool has_more_results) {
  CHECK(!current_callback_.is_null())
      << "Results were obtained after \"no more results\"";
  const PickerSectionType section_type = SectionTypeFromSearchSource(source);
  // Suggested results have multiple sources, which we store in any order and
  // explicitly do not append if post-burn-in.
  if (section_type == PickerSectionType::kNone) {
    // Suggested results cannot have more results, since it's not a proper
    // category.
    CHECK(!has_more_results);
    base::ranges::move(
        results,
        std::back_inserter(results_[PickerSectionType::kNone].results));
    return;
  }

  if (IsPostBurnIn()) {
    // Publish post-burn-in results and skip assignment.
    if (!results.empty()) {
      std::vector<PickerSearchResultsSection> sections;
      sections.emplace_back(section_type, std::move(results), has_more_results);
      current_callback_.Run(std::move(sections));
    }
    return;
  }

  const auto& [unused, inserted] = results_.emplace(
      section_type, PickerSearchResults(std::move(results), has_more_results));
  CHECK(inserted);
}

void PickerSearchAggregator::HandleNoMoreResults(bool interrupted) {
  // Only call the callback if it wasn't interrupted.
  if (!interrupted) {
    // We could get a "no more results" signal before burn-in finishes.
    // Publish those results immediately if that is the case.
    if (burn_in_timer_.IsRunning()) {
      burn_in_timer_.FireNow();
    }
    current_callback_.Run({});
  }
  // Ensure that we don't accidentally publish more results afterwards.
  current_callback_.Reset();
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
  base::flat_set<PickerSectionType> published_types;

  // The None section always goes first.
  if (auto it = results_.find(PickerSectionType::kNone);
      it != results_.end() && !it->second.results.empty()) {
    sections.emplace_back(PickerSectionType::kNone,
                          std::move(it->second.results),
                          /*has_more=*/false);
    published_types.insert(PickerSectionType::kNone);
  }

  // User generated results can be ranked amongst themselves.
  for (PickerSectionType type : {
           PickerSectionType::kLinks,
           PickerSectionType::kLocalFiles,
           PickerSectionType::kDriveFiles,
           PickerSectionType::kClipboard,
       }) {
    if (auto it = results_.find(type);
        it != results_.end() &&
        base::ranges::any_of(it->second.results, &ShouldPromote)) {
      sections.emplace_back(type, std::move(it->second.results),
                            it->second.has_more);
      published_types.insert(type);
    }
  }

  // The remaining results are ranked based on a predefined order
  for (PickerSectionType type : {
           PickerSectionType::kLinks,
           PickerSectionType::kLocalFiles,
           PickerSectionType::kDriveFiles,
           PickerSectionType::kClipboard,
           PickerSectionType::kEditorWrite,
           PickerSectionType::kEditorRewrite,
       }) {
    if (published_types.contains(type)) {
      continue;
    }
    if (auto it = results_.find(type);
        it != results_.end() && !it->second.results.empty()) {
      sections.emplace_back(type, std::move(it->second.results),
                            it->second.has_more);
    }
  }
  if (!sections.empty()) {
    current_callback_.Run(std::move(sections));
  }
}

base::WeakPtr<PickerSearchAggregator> PickerSearchAggregator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
