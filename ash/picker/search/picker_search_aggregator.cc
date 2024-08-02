// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_aggregator.h"

#include <iterator>
#include <utility>
#include <variant>
#include <vector>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/overloaded.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/substring_set_matcher/matcher_string_pattern.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/url_matcher/url_matcher.h"

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

void DeduplicateDriveLinksFromFiles(
    std::vector<PickerSearchResult>& links,
    base::span<const PickerSearchResult> files) {
  std::vector<base::MatcherStringPattern> patterns;
  base::MatcherStringPattern::ID next_id = 0;
  for (const PickerSearchResult& file : files) {
    auto* drive_data =
        std::get_if<PickerSearchResult::DriveFileData>(&file.data());
    if (drive_data == nullptr) {
      continue;
    }
    if (!drive_data->id.has_value()) {
      continue;
    }
    patterns.emplace_back(*drive_data->id, next_id);
    ++next_id;
  }

  base::SubstringSetMatcher matcher;
  bool success = matcher.Build(patterns);
  // This may fail if the tree gets too many nodes (`kInvalidNodeId`, which is
  // around ~8,400,000). Drive IDs are 44 characters long, so this would require
  // having >190,000 Drive IDs in the worst case. This should never happen.
  CHECK(success);

  std::erase_if(links, [&matcher](const PickerSearchResult& link) {
    auto* link_data =
        std::get_if<PickerSearchResult::BrowsingHistoryData>(&link.data());
    if (link_data == nullptr) {
      return false;
    }
    return matcher.AnyMatch(link_data->url.spec());
  });
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
  UnpublishedResults& accumulated =
      accumulated_results_[base::to_underlying(section_type)];
  // Suggested results have multiple sources, which we store in any order and
  // explicitly do not append if post-burn-in.
  if (section_type == PickerSectionType::kNone) {
    // Suggested results cannot have more results, since it's not a proper
    // category.
    CHECK(!has_more_results);
    base::ranges::move(results, std::back_inserter(accumulated.results));
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

  CHECK(accumulated.results.empty());
  accumulated = UnpublishedResults(std::move(results), has_more_results);
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

PickerSearchAggregator::UnpublishedResults::UnpublishedResults() = default;

PickerSearchAggregator::UnpublishedResults::UnpublishedResults(
    std::vector<PickerSearchResult> results,
    bool has_more)
    : results(std::move(results)), has_more(has_more) {}

PickerSearchAggregator::UnpublishedResults::UnpublishedResults(
    UnpublishedResults&& other) = default;

PickerSearchAggregator::UnpublishedResults&
PickerSearchAggregator::UnpublishedResults::operator=(
    UnpublishedResults&& other) = default;

PickerSearchAggregator::UnpublishedResults::~UnpublishedResults() = default;

bool PickerSearchAggregator::IsPostBurnIn() const {
  return !burn_in_timer_.IsRunning();
}

void PickerSearchAggregator::PublishBurnInResults() {
  if (UnpublishedResults* link_results =
          AccumulatedResultsForSection(PickerSectionType::kLinks)) {
    if (UnpublishedResults* drive_results =
            AccumulatedResultsForSection(PickerSectionType::kDriveFiles)) {
      DeduplicateDriveLinksFromFiles(link_results->results,
                                     drive_results->results);
    }
  }

  std::vector<PickerSearchResultsSection> sections;
  base::flat_set<PickerSectionType> published_types;

  // The None section always goes first.
  if (UnpublishedResults* none_results =
          AccumulatedResultsForSection(PickerSectionType::kNone)) {
    sections.emplace_back(PickerSectionType::kNone,
                          std::move(none_results->results),
                          /*has_more=*/false);
    published_types.insert(PickerSectionType::kNone);
  }

  // User generated results can be ranked amongst themselves.
  for (PickerSectionType type : {
           PickerSectionType::kLinks,
           PickerSectionType::kDriveFiles,
           PickerSectionType::kLocalFiles,
           PickerSectionType::kClipboard,
       }) {
    if (UnpublishedResults* results = AccumulatedResultsForSection(type);
        results && base::ranges::any_of(results->results, &ShouldPromote)) {
      sections.emplace_back(type, std::move(results->results),
                            results->has_more);
      published_types.insert(type);
    }
  }

  // The remaining results are ranked based on a predefined order
  for (PickerSectionType type : {
           PickerSectionType::kLinks,
           PickerSectionType::kDriveFiles,
           PickerSectionType::kLocalFiles,
           PickerSectionType::kClipboard,
           PickerSectionType::kEditorWrite,
           PickerSectionType::kEditorRewrite,
       }) {
    if (published_types.contains(type)) {
      continue;
    }
    if (UnpublishedResults* results = AccumulatedResultsForSection(type)) {
      sections.emplace_back(type, std::move(results->results),
                            results->has_more);
    }
  }
  if (!sections.empty()) {
    current_callback_.Run(std::move(sections));
  }
}

base::WeakPtr<PickerSearchAggregator> PickerSearchAggregator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

PickerSearchAggregator::UnpublishedResults*
PickerSearchAggregator::AccumulatedResultsForSection(PickerSectionType type) {
  UnpublishedResults& accumulated =
      accumulated_results_[base::to_underlying(type)];
  if (accumulated.results.empty()) {
    return nullptr;
  }
  return &accumulated;
}

}  // namespace ash
