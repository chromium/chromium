// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_SEARCH_CONTROLLER_H_
#define ASH_PICKER_PICKER_SEARCH_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/picker_search_debouncer.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/emoji/emoji_search.h"

namespace ash {

enum class AppListSearchResultType;
class PickerClient;

class ASH_EXPORT PickerSearchController {
 public:
  explicit PickerSearchController(PickerClient* client,
                                  base::TimeDelta burn_in_period);
  PickerSearchController(const PickerSearchController&) = delete;
  PickerSearchController& operator=(const PickerSearchController&) = delete;
  ~PickerSearchController();

  static constexpr base::TimeDelta kGifDebouncingDelay =
      base::Milliseconds(200);

  void StartSearch(const std::u16string& query,
                   std::optional<PickerCategory> category,
                   PickerViewDelegate::SearchResultsCallback callback);

 private:
  // Whether the burn-in period has ended for the current search.
  bool IsPostBurnIn() const;

  void StartGifSearch(const std::string& query);

  void ResetResults();
  void PublishBurnInResults();
  void AppendPostBurnInResults(PickerSearchResults::Section section);
  void HandleCrosSearchResults(ash::AppListSearchResultType type,
                               std::vector<PickerSearchResult> results);
  void HandleGifSearchResults(std::string query,
                              std::vector<PickerSearchResult> results);
  void HandleEmojiSearchResults(emoji::EmojiSearchResult results);

  const raw_ref<PickerClient> client_;

  base::TimeDelta burn_in_period_;
  base::OneShotTimer burn_in_timer_;

  emoji::EmojiSearch emoji_search_;

  std::string current_query_;
  PickerViewDelegate::SearchResultsCallback current_callback_;

  std::vector<PickerSearchResult> omnibox_results_;
  std::vector<PickerSearchResult> gif_results_;
  std::vector<PickerSearchResult> emoji_results_;

  PickerSearchDebouncer gif_search_debouncer_;

  base::WeakPtrFactory<PickerSearchController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_SEARCH_CONTROLLER_H_
