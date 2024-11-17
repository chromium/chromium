// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_search_aggregator.h"

#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/search/quick_insert_search_source.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
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

QuickInsertSectionType SectionTypeFromSearchSource(
    QuickInsertSearchSource source) {
  switch (source) {
    case QuickInsertSearchSource::kOmnibox:
      return QuickInsertSectionType::kLinks;
    case QuickInsertSearchSource::kDate:
    case QuickInsertSearchSource::kMath:
      return QuickInsertSectionType::kNone;
    case QuickInsertSearchSource::kClipboard:
      return QuickInsertSectionType::kClipboard;
    case QuickInsertSearchSource::kAction:
      return QuickInsertSectionType::kNone;
    case QuickInsertSearchSource::kLocalFile:
      return QuickInsertSectionType::kLocalFiles;
    case QuickInsertSearchSource::kDrive:
      return QuickInsertSectionType::kDriveFiles;
    case QuickInsertSearchSource::kEditorWrite:
    case QuickInsertSearchSource::kEditorRewrite:
    case QuickInsertSearchSource::kLobsterWithNoSelectedText:
    case QuickInsertSearchSource::kLobsterWithSelectedText:
      return QuickInsertSectionType::kContentEditor;
  }
}

bool ShouldPromote(const QuickInsertSearchResult& result) {
  return std::visit(
      base::Overloaded{
          [](const QuickInsertClipboardResult& data) { return data.is_recent; },
          [](const QuickInsertBrowsingHistoryResult& data) {
            return data.best_match;
          },
          [](const QuickInsertLocalFileResult& data) {
            return data.best_match;
          },
          [](const QuickInsertDriveFileResult& data) {
            return data.best_match;
          },
          [](const auto& data) { return false; }},
      result);
}

std::vector<GURL> LinksFromSearchResults(
    base::span<const QuickInsertSearchResult> results) {
  std::vector<GURL> links;
  for (const QuickInsertSearchResult& link : results) {
    auto* link_data = std::get_if<QuickInsertBrowsingHistoryResult>(&link);
    if (link_data == nullptr) {
      continue;
    }
    links.push_back(link_data->url);
  }
  return links;
}

std::vector<std::string> DriveIdsFromSearchResults(
    base::span<const QuickInsertSearchResult> results) {
  std::vector<std::string> drive_ids;
  for (const QuickInsertSearchResult& file : results) {
    auto* drive_data = std::get_if<QuickInsertDriveFileResult>(&file);
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

void DeduplicateDriveLinksFromIds(std::vector<QuickInsertSearchResult>& links,
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

  std::erase_if(links, [&matcher](const QuickInsertSearchResult& link) {
    auto* link_data = std::get_if<QuickInsertBrowsingHistoryResult>(&link);
    if (link_data == nullptr) {
      return false;
    }
    return matcher.AnyMatch(link_data->url.spec());
  });
}

void DeduplicateDriveFilesFromLinks(std::vector<QuickInsertSearchResult>& files,
                                    base::span<const GURL> links) {
  std::vector<base::MatcherStringPattern> patterns;
  for (size_t i = 0; i < files.size(); ++i) {
    auto* drive_data = std::get_if<QuickInsertDriveFileResult>(&files[i]);
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
  std::vector<QuickInsertSearchResult*> results_to_remove;
  for (size_t i : matched_files) {
    results_to_remove.push_back(&files[i]);
  }
  base::flat_set<QuickInsertSearchResult*> results_to_remove_set =
      std::move(results_to_remove);

  std::erase_if(files,
                [&results_to_remove_set](const QuickInsertSearchResult& file) {
                  return results_to_remove_set.contains(&file);
                });
}

}  // namespace

QuickInsertSearchAggregator::QuickInsertSearchAggregator(
    base::TimeDelta burn_in_period,
    QuickInsertViewDelegate::SearchResultsCallback callback) {
  current_callback_ = std::move(callback);
  CHECK(!current_callback_.is_null());

  // TODO: b/324154537 - Show a loading animation while waiting for results.
  burn_in_timer_.Start(FROM_HERE, burn_in_period, this,
                       &QuickInsertSearchAggregator::PublishBurnInResults);
}

QuickInsertSearchAggregator::~QuickInsertSearchAggregator() = default;

void QuickInsertSearchAggregator::HandleSearchSourceResults(
    QuickInsertSearchSource source,
    std::vector<QuickInsertSearchResult> results,
    bool has_more_results) {
  CHECK(!current_callback_.is_null())
      << "Results were obtained after \"no more results\"";
  const QuickInsertSectionType section_type =
      SectionTypeFromSearchSource(source);
  UnpublishedResults& accumulated =
      accumulated_results_[base::to_underlying(section_type)];
  // Suggested results have multiple sources, which we store in any order and
  // explicitly do not append if post-burn-in.
  if (section_type == QuickInsertSectionType::kNone ||
      section_type == QuickInsertSectionType::kContentEditor) {
    // Suggested results cannot have more results, since it's not a proper
    // category.
    CHECK(!has_more_results);
    base::ranges::move(results, std::back_inserter(accumulated.results));
    return;
  }

  if (IsPostBurnIn()) {
    // Publish post-burn-in results and skip assignment.
    if (!results.empty()) {
      if (section_type == QuickInsertSectionType::kDriveFiles) {
        if (std::holds_alternative<std::monostate>(link_drive_dedupe_state_)) {
          link_drive_dedupe_state_ = DriveIdsFromSearchResults(results);
        } else if (auto* links = std::get_if<std::vector<GURL>>(
                       &link_drive_dedupe_state_)) {
          DeduplicateDriveFilesFromLinks(results, std::move(*links));
          link_drive_dedupe_state_ = std::monostate();
        } else {
          NOTREACHED();
        }
      } else if (section_type == QuickInsertSectionType::kLinks) {
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

      std::vector<QuickInsertSearchResultsSection> sections;
      sections.emplace_back(section_type, std::move(results), has_more_results);
      current_callback_.Run(std::move(sections));
    }
    return;
  }

  CHECK(accumulated.results.empty());
  accumulated = UnpublishedResults(std::move(results), has_more_results);
}

void QuickInsertSearchAggregator::HandleNoMoreResults(bool interrupted) {
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

QuickInsertSearchAggregator::UnpublishedResults::UnpublishedResults() = default;

QuickInsertSearchAggregator::UnpublishedResults::UnpublishedResults(
    std::vector<QuickInsertSearchResult> results,
    bool has_more)
    : results(std::move(results)), has_more(has_more) {}

QuickInsertSearchAggregator::UnpublishedResults::UnpublishedResults(
    UnpublishedResults&& other) = default;

QuickInsertSearchAggregator::UnpublishedResults&
QuickInsertSearchAggregator::UnpublishedResults::operator=(
    UnpublishedResults&& other) = default;

QuickInsertSearchAggregator::UnpublishedResults::~UnpublishedResults() =
    default;

bool QuickInsertSearchAggregator::IsPostBurnIn() const {
  return !burn_in_timer_.IsRunning();
}

void QuickInsertSearchAggregator::PublishBurnInResults() {
  // This variable should only be set after burn-in.
  CHECK(std::holds_alternative<std::monostate>(link_drive_dedupe_state_));

  UnpublishedResults* link_results =
      AccumulatedResultsForSection(QuickInsertSectionType::kLinks);
  UnpublishedResults* drive_results =
      AccumulatedResultsForSection(QuickInsertSectionType::kDriveFiles);
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

  std::vector<QuickInsertSearchResultsSection> sections;
  base::flat_set<QuickInsertSectionType> published_types;

  // The None section always goes first.
  if (UnpublishedResults* none_results =
          AccumulatedResultsForSection(QuickInsertSectionType::kNone)) {
    sections.emplace_back(QuickInsertSectionType::kNone,
                          std::move(none_results->results),
                          /*has_more=*/false);
    published_types.insert(QuickInsertSectionType::kNone);
  }

  // User generated results can be ranked amongst themselves.
  for (QuickInsertSectionType type : {
           QuickInsertSectionType::kLinks,
           QuickInsertSectionType::kDriveFiles,
           QuickInsertSectionType::kLocalFiles,
           QuickInsertSectionType::kClipboard,
       }) {
    if (UnpublishedResults* results = AccumulatedResultsForSection(type);
        results && base::ranges::any_of(results->results, &ShouldPromote)) {
      sections.emplace_back(type, std::move(results->results),
                            results->has_more);
      published_types.insert(type);
    }
  }

  // The remaining results are ranked based on a predefined order
  for (QuickInsertSectionType type : {
           QuickInsertSectionType::kLinks,
           QuickInsertSectionType::kDriveFiles,
           QuickInsertSectionType::kLocalFiles,
           QuickInsertSectionType::kClipboard,
           QuickInsertSectionType::kContentEditor,
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

base::WeakPtr<QuickInsertSearchAggregator>
QuickInsertSearchAggregator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

QuickInsertSearchAggregator::UnpublishedResults*
QuickInsertSearchAggregator::AccumulatedResultsForSection(
    QuickInsertSectionType type) {
  UnpublishedResults& accumulated =
      accumulated_results_[base::to_underlying(type)];
  if (accumulated.results.empty()) {
    return nullptr;
  }
  return &accumulated;
}

}  // namespace ash
