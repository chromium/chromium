// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

class PickerSearchResult;

// Aggregates search results for a single Picker search request, including
// managing the order of search results and managing when to publish search
// results (with burn-in logic).
// Call `HandleSearchSourceResults` with new results once they arrive.
// Any timers start immediately once this class is constructed.
class ASH_EXPORT PickerSearchAggregator {
 public:
  explicit PickerSearchAggregator(
      base::TimeDelta burn_in_period,
      PickerViewDelegate::SearchResultsCallback callback);
  PickerSearchAggregator(const PickerSearchAggregator&) = delete;
  PickerSearchAggregator& operator=(const PickerSearchAggregator&) = delete;
  ~PickerSearchAggregator();

  void HandleSearchSourceResults(PickerSearchSource source,
                                 std::vector<PickerSearchResult> results,
                                 bool has_more_results);

  base::WeakPtr<PickerSearchAggregator> GetWeakPtr();

 private:
  struct PickerSearchResults {
    PickerSearchResults();
    PickerSearchResults(std::vector<PickerSearchResult> results, bool has_more);
    PickerSearchResults(PickerSearchResults&& other);
    PickerSearchResults& operator=(PickerSearchResults&& other);
    ~PickerSearchResults();

    std::vector<PickerSearchResult> results;
    bool has_more = false;
  };

  // Whether the burn-in period has ended for the current search.
  bool IsPostBurnIn() const;

  void PublishBurnInResults();

  void HandleSearchSourceResultsImpl(PickerSearchSource source,
                                     std::vector<PickerSearchResult> results,
                                     bool has_more_results);

  base::OneShotTimer burn_in_timer_;

  PickerViewDelegate::SearchResultsCallback current_callback_;

  base::flat_map<PickerSectionType, PickerSearchResults> results_;

  // Whether the drive search has finished (either successfully or with an
  // error).
  bool drive_search_finished_ = false;

  // This holds the results of the GIF search in the case where GIF search
  // finishes before Drive search. GIF results must appear later than Drive
  // results, so they are kept here until Drive search finishes.
  std::optional<std::vector<PickerSearchResult>> pending_gif_results_;

  base::WeakPtrFactory<PickerSearchAggregator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_
