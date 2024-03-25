// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
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
                                 std::vector<PickerSearchResult> results);

  base::WeakPtr<PickerSearchAggregator> GetWeakPtr();

 private:
  // Whether the burn-in period has ended for the current search.
  bool IsPostBurnIn() const;

  void PublishBurnInResults();

  base::TimeDelta burn_in_period_;
  base::OneShotTimer burn_in_timer_;

  PickerViewDelegate::SearchResultsCallback current_callback_;

  std::vector<PickerSearchResult> category_results_;
  std::vector<PickerSearchResult> suggested_results_;
  std::vector<PickerSearchResult> omnibox_results_;
  std::vector<PickerSearchResult> gif_results_;
  std::vector<PickerSearchResult> emoji_results_;
  std::vector<PickerSearchResult> local_file_results_;
  std::vector<PickerSearchResult> drive_file_results_;

  base::WeakPtrFactory<PickerSearchAggregator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_AGGREGATOR_H_
