// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_CONTROLLER_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/search/picker_search_aggregator.h"
#include "ash/picker/search/picker_search_request.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chromeos/ash/components/emoji/emoji_search.h"

namespace ash {

class PickerClient;

class ASH_EXPORT PickerSearchController {
 public:
  explicit PickerSearchController(
      PickerClient* client,
      base::TimeDelta burn_in_period);
  PickerSearchController(const PickerSearchController&) = delete;
  PickerSearchController& operator=(const PickerSearchController&) = delete;
  ~PickerSearchController();

  void StartSearch(std::u16string_view query,
                   std::optional<PickerCategory> category,
                   PickerSearchRequest::Options search_options,
                   PickerViewDelegate::SearchResultsCallback callback);

  void StopSearch();

  void StartEmojiSearch(
      std::u16string_view query,
      PickerViewDelegate::EmojiSearchResultsCallback callback);

 private:
  const raw_ref<PickerClient> client_;

  base::TimeDelta burn_in_period_;

  emoji::EmojiSearch emoji_search_;
  // The search request calls the aggregator, so the search request should be
  // destructed first.
  std::unique_ptr<PickerSearchAggregator> aggregator_;
  std::unique_ptr<PickerSearchRequest> search_request_;
};

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_CONTROLLER_H_
