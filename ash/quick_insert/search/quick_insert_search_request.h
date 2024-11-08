// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_REQUEST_H_
#define ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_REQUEST_H_

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/search/quick_insert_search_debouncer.h"
#include "ash/quick_insert/search/quick_insert_search_source.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"

namespace ash {

class QuickInsertClient;
class PickerClipboardHistoryProvider;

// Represents a single Quick Insert search query. Constructing this class starts
// a search, and destructing it stops the search.
class ASH_EXPORT QuickInsertSearchRequest {
 public:
  using SearchResultsCallback =
      base::RepeatingCallback<void(QuickInsertSearchSource source,
                                   std::vector<QuickInsertSearchResult> results,
                                   bool has_more_results)>;
  using DoneCallback = base::OnceCallback<void(bool interrupted)>;

  // `done_closure` is guaranteed to be called strictly after the last call to
  // `callback`.
  QuickInsertSearchRequest(
      std::u16string_view query,
      std::optional<QuickInsertCategory> category,
      SearchResultsCallback callback,
      DoneCallback done_callback,
      QuickInsertClient* client,
      base::span<const QuickInsertCategory> available_categories = {},
      bool caps_lock_state_to_search = false,
      bool search_case_transforms = false);
  QuickInsertSearchRequest(const QuickInsertSearchRequest&) = delete;
  QuickInsertSearchRequest& operator=(const QuickInsertSearchRequest&) = delete;
  ~QuickInsertSearchRequest();

 private:
  void HandleSearchSourceResults(QuickInsertSearchSource source,
                                 std::vector<QuickInsertSearchResult> results,
                                 bool has_more_results);

  void HandleActionSearchResults(std::vector<QuickInsertSearchResult> results);
  void HandleCrosSearchResults(ash::AppListSearchResultType type,
                               std::vector<QuickInsertSearchResult> results);
  void HandleDateSearchResults(std::vector<QuickInsertSearchResult> results);
  void HandleMathSearchResults(std::optional<QuickInsertSearchResult> result);
  void HandleClipboardSearchResults(
      std::vector<QuickInsertSearchResult> results);
  void HandleEditorSearchResults(QuickInsertSearchSource source,
                                 std::optional<QuickInsertSearchResult> result);
  void HandleLobsterSearchResults(
      QuickInsertSearchSource source,
      std::optional<QuickInsertSearchResult> result);

  // Sets the search for the source to be started right now.
  // `CHECK` fails if a search was already started.
  void MarkSearchStarted(QuickInsertSearchSource source);
  // Sets the search for the source to be not started, and emits a metric for
  // the source.
  // `CHECK` fails if a search wasn't started.
  void MarkSearchEnded(QuickInsertSearchSource source);
  std::optional<base::TimeTicks> SwapSearchStart(
      QuickInsertSearchSource source,
      std::optional<base::TimeTicks> new_value);

  void MaybeCallDoneClosure();

  bool is_category_specific_search_;
  const raw_ref<QuickInsertClient> client_;

  std::unique_ptr<PickerClipboardHistoryProvider> clipboard_provider_;

  SearchResultsCallback current_callback_;
  // Set to true once all the searches have started at the end of the ctor.
  bool can_call_done_closure_ = false;
  // Guaranteed to be non-null in the ctor.
  // Guaranteed to be null after it is called - it will never be reassigned.
  // Once called, `current_callback_` will also be reset to null.
  DoneCallback done_callback_;

  static constexpr size_t kNumSources =
      base::to_underlying(QuickInsertSearchSource::kMaxValue) + 1;
  std::array<std::optional<base::TimeTicks>, kNumSources> search_starts_;

  base::WeakPtrFactory<QuickInsertSearchRequest> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_REQUEST_H_
