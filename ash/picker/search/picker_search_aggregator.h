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

class ASH_EXPORT PickerSearchAggregator {
 public:
  explicit PickerSearchAggregator(base::TimeDelta burn_in_period);
  PickerSearchAggregator(const PickerSearchAggregator&) = delete;
  PickerSearchAggregator& operator=(const PickerSearchAggregator&) = delete;
  ~PickerSearchAggregator();

  void StartSearch(PickerViewDelegate::SearchResultsCallback callback);

  void HandleSearchSourceResults(PickerSearchSource source,
                                 std::vector<PickerSearchResult> results);

  base::WeakPtr<PickerSearchAggregator> GetWeakPtr();

 private:
  // Stops the current search, and resets the state to begin a new search.
  // This is called in `StartSearch` before every new search query.
  void StopSearch();

  // Whether there is no current search. This could be because a search was
  // never started, or `StopSearch` was called (possibly as part of
  // `StartSearch`).
  // This is equivalent to whether the current callback is null.
  bool IsSearchStopped() const;

  // Whether the burn-in period has ended for the current search.
  bool IsPostBurnIn() const;

  void ResetResults();
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
