// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_

#include <array>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/cxx23_to_underlying.h"
#include "url/gurl.h"

namespace ash {


// Aggregates search results for a single Picker search request, including
// managing the order of search results and managing when to publish search
// results (with burn-in logic).
// Call `HandleSearchSourceResults` with new results once they arrive.
// Call `HandleNoMoreResults` once `HandleSearchSourceResults` will never be
// called again in the future.
// Any timers start immediately once this class is constructed.
class ASH_EXPORT PickerSearchAggregator {
 public:
  // If `callback` is called with empty results, then it will never be called
  // again (i.e. all search results have been returned).
  explicit PickerSearchAggregator(
      base::TimeDelta burn_in_period,
      PickerViewDelegate::SearchResultsCallback callback);
  PickerSearchAggregator(const PickerSearchAggregator&) = delete;
  PickerSearchAggregator& operator=(const PickerSearchAggregator&) = delete;
  ~PickerSearchAggregator();

  void HandleSearchSourceResults(PickerSearchSource source,
                                 std::vector<PickerSearchResult> results,
                                 bool has_more_results);
  void HandleNoMoreResults(bool interrupted);

  base::WeakPtr<PickerSearchAggregator> GetWeakPtr();

 private:
  struct UnpublishedResults {
    UnpublishedResults();
    UnpublishedResults(std::vector<PickerSearchResult> results, bool has_more);
    UnpublishedResults(UnpublishedResults&& other);
    UnpublishedResults& operator=(UnpublishedResults&& other);
    ~UnpublishedResults();

    std::vector<PickerSearchResult> results;
    bool has_more = false;
  };

  // Whether the burn-in period has ended for the current search.
  bool IsPostBurnIn() const;

  void PublishBurnInResults();

  // Returns nullptr if there are no accumulated results for the section type.
  UnpublishedResults* AccumulatedResultsForSection(PickerSectionType type);

  base::OneShotTimer burn_in_timer_;

  PickerViewDelegate::SearchResultsCallback current_callback_;

  static constexpr size_t kNumSections =
      base::to_underlying(PickerSectionType::kMaxValue) + 1;
  // Unpublished results that are accumulated before burn-in.
  // Results are only published after burn-in if the `results` vector is not
  // empty.
  std::array<UnpublishedResults, kNumSections> accumulated_results_;

  using LinkDriveDedupeState =
      std::variant<std::monostate,
                   /*post_burnin_and_links_only=*/std::vector<GURL>,
                   /*post_burnin_and_drive_only=*/std::vector<std::string>>;
  LinkDriveDedupeState link_drive_dedupe_state_;

  base::WeakPtrFactory<PickerSearchAggregator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_
