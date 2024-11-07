// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_CONTROLLER_H_
#define ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/search/quick_insert_search_aggregator.h"
#include "ash/quick_insert/search/quick_insert_search_request.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/emoji/emoji_search.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace ash {

class QuickInsertClient;

class ASH_EXPORT PickerSearchController {
 public:
  explicit PickerSearchController(base::TimeDelta burn_in_period);
  PickerSearchController(const PickerSearchController&) = delete;
  PickerSearchController& operator=(const PickerSearchController&) = delete;
  ~PickerSearchController();

  // Adds emoji keywords for enabled languages in `prefs` and whenever the
  // enabled languages change. This does not unload any keywords loaded
  // previously. `prefs` can be null, which stops this class from listening to
  // changes to prefs.
  void LoadEmojiLanguagesFromPrefs(PrefService* prefs);

  // `client` must remain valid until `StopSearch` is called or until this
  // object is destroyed.
  void StartSearch(QuickInsertClient* client,
                   std::u16string_view query,
                   std::optional<QuickInsertCategory> category,
                   base::span<const QuickInsertCategory> available_categories,
                   bool caps_lock_state_to_search,
                   bool search_case_transforms,
                   QuickInsertViewDelegate::SearchResultsCallback callback);

  void StopSearch();

  void StartEmojiSearch(
      PrefService* prefs,
      std::u16string_view query,
      QuickInsertViewDelegate::EmojiSearchResultsCallback callback);

  // Gets the emoji name for the given emoji / emoticon / symbol.
  // Used for getting emoji tooltips for zero state emoji.
  // TODO: b/358492493 - Refactor this out of `PickerSearchController`, as this
  // is unrelated to search.
  std::string GetEmojiName(std::string_view emoji);

 private:
  void LoadEmojiLanguages(PrefService* pref);

  PrefChangeRegistrar pref_change_registrar_;

  base::TimeDelta burn_in_period_;

  emoji::EmojiSearch emoji_search_;
  // The search request calls the aggregator, so the search request should be
  // destructed first.
  std::unique_ptr<QuickInsertSearchAggregator> aggregator_;
  std::unique_ptr<QuickInsertSearchRequest> search_request_;

  base::WeakPtrFactory<PickerSearchController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_CONTROLLER_H_
