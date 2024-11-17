// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_AGGREGATOR_H_
#define ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_AGGREGATOR_H_

#include <array>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/search/quick_insert_search_source.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/cxx23_to_underlying.h"
#include "url/gurl.h"

namespace ash {

// Aggregates search results for a single Quick Insert search request, including
// managing the order of search results and managing when to publish search
// results (with burn-in logic).
// Call `HandleSearchSourceResults` with new results once they arrive.
// Call `HandleNoMoreResults` once `HandleSearchSourceResults` will never be
// called again in the future.
// Any timers start immediately once this class is constructed.
class ASH_EXPORT QuickInsertSearchAggregator {
 public:
  // If `callback` is called with empty results, then it will never be called
  // again (i.e. all search results have been returned).
  explicit QuickInsertSearchAggregator(
      base::TimeDelta burn_in_period,
      QuickInsertViewDelegate::SearchResultsCallback callback);
  QuickInsertSearchAggregator(const QuickInsertSearchAggregator&) = delete;
  QuickInsertSearchAggregator& operator=(const QuickInsertSearchAggregator&) =
      delete;
  ~QuickInsertSearchAggregator();

  void HandleSearchSourceResults(QuickInsertSearchSource source,
                                 std::vector<QuickInsertSearchResult> results,
                                 bool has_more_results);
  void HandleNoMoreResults(bool interrupted);

  base::WeakPtr<QuickInsertSearchAggregator> GetWeakPtr();

 private:
  struct UnpublishedResults {
    UnpublishedResults();
    UnpublishedResults(std::vector<QuickInsertSearchResult> results,
                       bool has_more);
    UnpublishedResults(UnpublishedResults&& other);
    UnpublishedResults& operator=(UnpublishedResults&& other);
    ~UnpublishedResults();

    std::vector<QuickInsertSearchResult> results;
    bool has_more = false;
  };

  // Whether the burn-in period has ended for the current search.
  bool IsPostBurnIn() const;

  void PublishBurnInResults();

  // Returns nullptr if there are no accumulated results for the section type.
  UnpublishedResults* AccumulatedResultsForSection(QuickInsertSectionType type);

  base::OneShotTimer burn_in_timer_;

  QuickInsertViewDelegate::SearchResultsCallback current_callback_;

  static constexpr size_t kNumSections =
      base::to_underlying(QuickInsertSectionType::kMaxValue) + 1;
  // Unpublished results that are accumulated before burn-in.
  // Results are only published after burn-in if the `results` vector is not
  // empty.
  std::array<UnpublishedResults, kNumSections> accumulated_results_;

  using LinkDriveDedupeState =
      std::variant<std::monostate,
                   /*post_burnin_and_links_only=*/std::vector<GURL>,
                   /*post_burnin_and_drive_only=*/std::vector<std::string>>;
  LinkDriveDedupeState link_drive_dedupe_state_;

  base::WeakPtrFactory<QuickInsertSearchAggregator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_AGGREGATOR_H_
