// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/app_list/search/keyboard_shortcut_data.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

class Profile;

namespace app_list {

class KeyboardShortcutProvider : public SearchProvider {
 public:
  explicit KeyboardShortcutProvider(Profile* profile);
  ~KeyboardShortcutProvider() override;

  KeyboardShortcutProvider(const KeyboardShortcutProvider&) = delete;
  KeyboardShortcutProvider& operator=(const KeyboardShortcutProvider&) = delete;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  using ShortcutDataAndScores =
      std::vector<std::pair<KeyboardShortcutData, double>>;

  // Fetch the list of hardcoded shortcuts, process, and save into
  // |shortcut_data_|.
  void ProcessShortcutList();

  void OnSearchComplete(ShortcutDataAndScores);

  Profile* const profile_;

  // A full collection of keyboard shortcuts, against which a query is compared
  // during a search.
  std::vector<KeyboardShortcutData> shortcut_data_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<KeyboardShortcutProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_
