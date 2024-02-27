// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_

#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_data.h"
#include "chrome/browser/ash/app_list/search/manatee/manatee_cache.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

class Profile;

namespace app_list {

class KeyboardShortcutProvider : public SearchProvider {
 public:
  explicit KeyboardShortcutProvider(
      Profile* profile,
      std::unique_ptr<ManateeCache> manatee_cache);
  ~KeyboardShortcutProvider() override;

  KeyboardShortcutProvider(const KeyboardShortcutProvider&) = delete;
  KeyboardShortcutProvider& operator=(const KeyboardShortcutProvider&) = delete;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;
  // Allow a test class to inject a fake or mock search handler.
  void SetSearchHandlerForTesting(ash::shortcut_ui::SearchHandler* handler) {
    search_handler_ = handler;
  }
  // Callback function to be run after list of shortcuts is sent for
  // processing.
  void OnManateeShortcutsResponseCallback(
      std::vector<std::vector<double>>& reply);

  // Callback function for comparing the obtained query embedding with
  // Keyboard Shortcut embeddings to find the most relevant results.
  void OnManateeQueryResponseCallback(std::vector<std::vector<double>>& reply);

  // Registers |OnManateeQueryResponseCallback| function to |manatee_cache_|
  // and makes request to model with query.
  void InitializeManateeCacheForQuery(const std::string query);

  // Extracts descriptions from KeyboardShortcutData items to send to model
  // in a batch request.
  void InitializeManateeCacheForShortcuts();

  // Set |shortcut_data_| to a smaller list for testing purposes.
  void set_shortcut_data_for_test(
      std::vector<KeyboardShortcutData> test_shortcut_data) {
    shortcut_data_ = test_shortcut_data;
  }
  std::vector<KeyboardShortcutData> shortcut_data() { return shortcut_data_; }

  // Overrides `should_apply_query_filtering_` for testing purposes.
  void set_should_apply_query_filtering_for_test(
      bool should_apply_query_filtering) {
    should_apply_query_filtering_ = should_apply_query_filtering;
  }

 private:
  using ShortcutDataAndScores =
      std::vector<std::pair<KeyboardShortcutData, double>>;

  // Fetch the list of hardcoded shortcuts, process, and save into
  // |shortcut_data_|.
  void ProcessShortcutList();

  void OnSearchComplete(ShortcutDataAndScores);
  void OnShortcutsSearchComplete(
      std::vector<ash::shortcut_customization::mojom::SearchResultPtr>);

  std::u16string query_;

  const raw_ptr<Profile> profile_;

  std::unique_ptr<ManateeCache> manatee_cache_;

  // A check for whether the |embedding_| field of KeyboardShortcutData has been
  // set.
  bool is_embeddings_set_ = false;

  // A check for whether we should apply query filtering for keyboard shortcut
  // results.
  bool should_apply_query_filtering_ = false;

  // A full collection of keyboard shortcuts, against which a query is compared
  // during a search.
  std::vector<KeyboardShortcutData> shortcut_data_;
  // The |search_handler_| is managed by ShortcutsAppManager which is
  // implemented as a KeyedService, active for the lifetime of a logged-in user.
  raw_ptr<ash::shortcut_ui::SearchHandler> search_handler_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<KeyboardShortcutProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_
