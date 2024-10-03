// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/search/picker_search_debouncer.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"

namespace ash {

class PickerClient;
class PickerClipboardHistoryProvider;

// Represents a single Picker search query. Constructing this class starts a
// search, and destructing it stops the search.
class ASH_EXPORT PickerSearchRequest {
 public:
  using SearchResultsCallback =
      base::RepeatingCallback<void(PickerSearchSource source,
                                   std::vector<PickerSearchResult> results,
                                   bool has_more_results)>;
  using DoneCallback = base::OnceCallback<void(bool interrupted)>;

  struct Options {
    base::span<const PickerCategory> available_categories;
    bool caps_lock_state_to_search = false;
    bool search_case_transforms = false;
  };

  // `done_closure` is guaranteed to be called strictly after the last call to
  // `callback`.
  explicit PickerSearchRequest(std::u16string_view query,
                               std::optional<PickerCategory> category,
                               SearchResultsCallback callback,
                               DoneCallback done_callback,
                               PickerClient* client,
                               const Options& options);
  PickerSearchRequest(const PickerSearchRequest&) = delete;
  PickerSearchRequest& operator=(const PickerSearchRequest&) = delete;
  ~PickerSearchRequest();

 private:
  void HandleSearchSourceResults(PickerSearchSource source,
                                 std::vector<PickerSearchResult> results,
                                 bool has_more_results);

  void HandleActionSearchResults(std::vector<PickerSearchResult> results);
  void HandleCrosSearchResults(ash::AppListSearchResultType type,
                               std::vector<PickerSearchResult> results);
  void HandleDateSearchResults(std::vector<PickerSearchResult> results);
  void HandleMathSearchResults(std::optional<PickerSearchResult> result);
  void HandleClipboardSearchResults(std::vector<PickerSearchResult> results);
  void HandleEditorSearchResults(PickerSearchSource source,
                                 std::optional<PickerSearchResult> result);
  void HandleLobsterSearchResults(PickerSearchSource source,
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

  void MaybeCallDoneClosure();

  bool is_category_specific_search_;
  const raw_ref<PickerClient> client_;

  std::unique_ptr<PickerClipboardHistoryProvider> clipboard_provider_;

  SearchResultsCallback current_callback_;
  // Set to true once all the searches have started at the end of the ctor.
  bool can_call_done_closure_ = false;
  // Guaranteed to be non-null in the ctor.
  // Guaranteed to be null after it is called - it will never be reassigned.
  // Once called, `current_callback_` will also be reset to null.
  DoneCallback done_callback_;

  static constexpr size_t kNumSources =
      base::to_underlying(PickerSearchSource::kMaxValue) + 1;
  std::array<std::optional<base::TimeTicks>, kNumSources> search_starts_;

  base::WeakPtrFactory<PickerSearchRequest> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_
