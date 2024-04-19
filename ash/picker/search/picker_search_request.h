// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_

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
#include "chromeos/ash/components/emoji/emoji_search.h"

namespace emoji {
class EmojiSearch;
}

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
      emoji::EmojiSearch* emoji_search,
      base::span<const PickerCategory> available_categories);
  PickerSearchRequest(const PickerSearchRequest&) = delete;
  PickerSearchRequest& operator=(const PickerSearchRequest&) = delete;
  ~PickerSearchRequest();

  static constexpr base::TimeDelta kGifDebouncingDelay =
      base::Milliseconds(200);
  static constexpr base::TimeDelta kDriveSearchTimeout = base::Seconds(1);

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
  void HandleEmojiSearchResults(emoji::EmojiSearchResult results);
  void HandleDateSearchResults(std::vector<PickerSearchResult> results);
  void HandleMathSearchResults(std::optional<PickerSearchResult> result);
  void HandleClipboardSearchResults(std::vector<PickerSearchResult> results);
  void HandleEditorSearchResults(PickerSearchSource source,
                                 std::optional<PickerSearchResult> result);

  void OnDriveSearchTimeout();

  bool is_category_specific_search_;
  const raw_ref<PickerClient> client_;

  std::unique_ptr<PickerClipboardProvider> clipboard_provider_;

  const raw_ref<emoji::EmojiSearch> emoji_search_;

  SearchResultsCallback current_callback_;

  std::optional<base::TimeTicks> date_search_start_;
  std::optional<base::TimeTicks> cros_search_start_;
  std::optional<base::TimeTicks> gif_search_start_;
  std::optional<base::TimeTicks> emoji_search_start_;
  std::optional<base::TimeTicks> category_search_start_;
  std::optional<base::TimeTicks> math_search_start_;
  std::optional<base::TimeTicks> clipboard_search_start_;
  std::optional<base::TimeTicks> editor_search_start_;

  PickerSearchDebouncer gif_search_debouncer_;

  base::OneShotTimer drive_search_timeout_timer_;

  base::WeakPtrFactory<PickerSearchRequest> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_REQUEST_H_
