// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/search/picker_search_debouncer.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"

namespace ash {

class PickerClient;
class PickerClipboardProvider;

// Represents a single Picker search query. Constructing this class starts a
// search, and destructing it stops the search.
class ASH_EXPORT PickerSearchRequest {
 public:
  using SearchResultsCallback =
      base::RepeatingCallback<void(PickerSearchSource source,
                                   std::vector<PickerSearchResult> results,
                                   bool has_more_results)>;

  explicit PickerSearchRequest(
      const std::u16string& query,
      std::optional<PickerCategory> category,
      SearchResultsCallback callback,
      PickerClient* client,
      base::span<const PickerCategory> available_categories);
  PickerSearchRequest(const PickerSearchRequest&) = delete;
  PickerSearchRequest& operator=(const PickerSearchRequest&) = delete;
  ~PickerSearchRequest();

  static constexpr base::TimeDelta kGifDebouncingDelay =
      base::Milliseconds(200);

 private:
  void StartGifSearch(const std::string& query);

  void HandleSearchSourceResults(PickerSearchSource source,
                                 std::vector<PickerSearchResult> results,
                                 bool has_more_results);

  void HandleCategorySearchResults(std::vector<PickerSearchResult> results);
  void HandleCrosSearchResults(ash::AppListSearchResultType type,
                               std::vector<PickerSearchResult> results);
  void HandleGifSearchResults(std::string query,
                              std::vector<PickerSearchResult> results);
  void HandleDateSearchResults(std::vector<PickerSearchResult> results);
  void HandleMathSearchResults(std::optional<PickerSearchResult> result);
  void HandleClipboardSearchResults(std::vector<PickerSearchResult> results);
  void HandleEditorSearchResults(PickerSearchSource source,
                                 std::optional<PickerSearchResult> result);

  // Sets the search for the source to be started right now.
  // `CHECK` fails if a search was already started.
  void MarkSearchStarted(PickerSearchSource source);
  // Sets the search for the source to be not started, and emits a metric for
  // the source.
  // `CHECK` fails if a search wasn't started.
  void MarkSearchEnded(PickerSearchSource source);
  std::optional<base::TimeTicks> SwapSearchStart(
      PickerSearchSource source,
      std::optional<base::TimeTicks> new_value);

  bool is_category_specific_search_;
  const raw_ref<PickerClient> client_;

  std::unique_ptr<PickerClipboardProvider> clipboard_provider_;

  SearchResultsCallback current_callback_;

  static constexpr size_t kNumSources =
      base::to_underlying(PickerSearchSource::kMaxValue) + 1;
  std::array<std::optional<base::TimeTicks>, kNumSources> search_starts_;

  PickerSearchDebouncer gif_search_debouncer_;

  base::WeakPtrFactory<PickerSearchRequest> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_
