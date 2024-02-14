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
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace ash {

enum class AppListSearchResultType;
class PickerClient;

class ASH_EXPORT PickerSearchController {
 public:
  explicit PickerSearchController(PickerClient* client);
  PickerSearchController(const PickerSearchController&) = delete;
  PickerSearchController& operator=(const PickerSearchController&) = delete;
  ~PickerSearchController();

  void StartSearch(const std::u16string& query,
                   std::optional<PickerCategory> category,
                   PickerViewDelegate::SearchResultsCallback callback);

 private:
  void ResetResults();
  void RunCallback();
  void HandleSearchResults(ash::AppListSearchResultType type,
                           std::vector<PickerSearchResult> results);
  void HandleGifSearchResults(std::u16string query,
                              std::vector<PickerSearchResult> results);

  const raw_ref<PickerClient> client_;

  std::vector<PickerSearchResult> omnibox_results_;
  std::vector<PickerSearchResult> gif_results_;
  std::u16string current_query_;
  PickerViewDelegate::SearchResultsCallback current_callback_;
  base::WeakPtrFactory<PickerSearchController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_SEARCH_CONTROLLER_H_
