// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_aggregator.h"

#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
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
    case PickerSearchSource::kEditorRewrite:
    case PickerSearchSource::kLobster:
      return PickerSectionType::kContentEditor;
  }
}

bool ShouldPromote(const PickerSearchResult& result) {
  return std::visit(
      base::Overloaded{
          [](const PickerClipboardResult& data) { return data.is_recent; },
          [](const PickerBrowsingHistoryResult& data) {
            return data.best_match;
          },
          [](const PickerLocalFileResult& data) { return data.best_match; },
          [](const PickerDriveFileResult& data) { return data.best_match; },
          [](const auto& data) { return false; }},
      result);
}

std::vector<GURL> LinksFromSearchResults(
    base::span<const PickerSearchResult> results) {
  std::vector<GURL> links;
  for (const PickerSearchResult& link : results) {
    auto* link_data = std::get_if<PickerBrowsingHistoryResult>(&link);
    if (link_data == nullptr) {
      continue;
    }
    links.push_back(link_data->url);
  }
  return links;
}

std::vector<std::string> DriveIdsFromSearchResults(
    base::span<const PickerSearchResult> results) {
  std::vector<std::string> drive_ids;
  for (const PickerSearchResult& file : results) {
    auto* drive_data = std::get_if<PickerDriveFileResult>(&file);
    if (drive_data == nullptr) {
      continue;
    }
    if (!drive_data->id.has_value()) {
      continue;
    }
    drive_ids.push_back(*drive_data->id);
  }
  return drive_ids;
}

void DeduplicateDriveLinksFromIds(std::vector<PickerSearchResult>& links,
                                  std::vector<std::string> drive_ids) {
  std::vector<base::MatcherStringPattern> patterns;
  base::MatcherStringPattern::ID next_id = 0;
  for (std::string& drive_id : drive_ids) {
    patterns.emplace_back(std::move(drive_id), next_id);
    ++next_id;
  }

  base::SubstringSetMatcher matcher;
  bool success = matcher.Build(patterns);
  // This may fail if the tree gets too many nodes (`kInvalidNodeId`, which is
  // around ~8,400,000). Drive IDs are 44 characters long, so this would require
  // having >190,000 Drive IDs in the worst case. This should never happen.
  CHECK(success);

  std::erase_if(links, [&matcher](const PickerSearchResult& link) {
    auto* link_data = std::get_if<PickerBrowsingHistoryResult>(&link);
    if (link_data == nullptr) {
      return false;
    }
    return matcher.AnyMatch(link_data->url.spec());
  });
}

void DeduplicateDriveFilesFromLinks(std::vector<PickerSearchResult>& files,
                                    base::span<const GURL> links) {
  std::vector<base::MatcherStringPattern> patterns;
  for (size_t i = 0; i < files.size(); ++i) {
    auto* drive_data = std::get_if<PickerDriveFileResult>(&files[i]);
    if (drive_data == nullptr) {
      continue;
    }
    if (!drive_data->id.has_value()) {
      continue;
    }
    // Pattern IDs need to be associated with the index of the file so we can
    // remove them below.
    patterns.emplace_back(*drive_data->id, i);
  }

  base::SubstringSetMatcher matcher;
  bool success = matcher.Build(patterns);
  CHECK(success);

  std::set<size_t> matched_files;
  for (const GURL& link : links) {
    // Drive IDs are unlikely to overlap as they are random fixed-length
    // strings, so the number of `matched_files` set insertions should be
    // limited to `O(t)` for each call.
    matcher.Match(link.spec(), &matched_files);
  }
  std::vector<PickerSearchResult*> results_to_remove;
  for (size_t i : matched_files) {
    results_to_remove.push_back(&files[i]);
  }
  base::flat_set<PickerSearchResult*> results_to_remove_set =
      std::move(results_to_remove);

  std::erase_if(files,
                [&results_to_remove_set](const PickerSearchResult& file) {
                  return results_to_remove_set.contains(&file);
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
  if (section_type == PickerSectionType::kNone ||
      section_type == PickerSectionType::kContentEditor) {
    // Suggested results cannot have more results, since it's not a proper
    // category.
    CHECK(!has_more_results);
    base::ranges::move(results, std::back_inserter(accumulated.results));
    return;
  }

  if (IsPostBurnIn()) {
    // Publish post-burn-in results and skip assignment.
    if (!results.empty()) {
      if (section_type == PickerSectionType::kDriveFiles) {
        if (std::holds_alternative<std::monostate>(link_drive_dedupe_state_)) {
          link_drive_dedupe_state_ = DriveIdsFromSearchResults(results);
        } else if (auto* links = std::get_if<std::vector<GURL>>(
                       &link_drive_dedupe_state_)) {
          DeduplicateDriveFilesFromLinks(results, std::move(*links));
          link_drive_dedupe_state_ = std::monostate();
        } else {
          NOTREACHED();
        }
      } else if (section_type == PickerSectionType::kLinks) {
        if (std::holds_alternative<std::monostate>(link_drive_dedupe_state_)) {
          link_drive_dedupe_state_ = LinksFromSearchResults(results);
        } else if (auto* drive_ids = std::get_if<std::vector<std::string>>(
                       &link_drive_dedupe_state_)) {
          DeduplicateDriveLinksFromIds(results, std::move(*drive_ids));
          link_drive_dedupe_state_ = std::monostate();
        } else {
          NOTREACHED();
        }
      }

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
  // This variable should only be set after burn-in.
  CHECK(std::holds_alternative<std::monostate>(link_drive_dedupe_state_));

  UnpublishedResults* link_results =
      AccumulatedResultsForSection(PickerSectionType::kLinks);
  UnpublishedResults* drive_results =
      AccumulatedResultsForSection(PickerSectionType::kDriveFiles);
  if (link_results != nullptr && drive_results != nullptr) {
    DeduplicateDriveLinksFromIds(
        link_results->results,
        DriveIdsFromSearchResults(drive_results->results));
  } else if (link_results != nullptr) {
    // Link results came in before burn-in, and Drive results didn't.
    link_drive_dedupe_state_ = LinksFromSearchResults(link_results->results);
  } else if (drive_results != nullptr) {
    // Drive results came in before burn-in, and link results didn't.
    link_drive_dedupe_state_ =
        DriveIdsFromSearchResults(drive_results->results);
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
           PickerSectionType::kContentEditor,
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
