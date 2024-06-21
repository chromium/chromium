// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_

#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

namespace app_list {

class KeyboardShortcutProvider
    : public SearchProvider,
      public session_manager::SessionManagerObserver {
 public:
  explicit KeyboardShortcutProvider(Profile* profile);
  ~KeyboardShortcutProvider() override;

  KeyboardShortcutProvider(const KeyboardShortcutProvider&) = delete;
  KeyboardShortcutProvider& operator=(const KeyboardShortcutProvider&) = delete;

  // Initialize the provider. It should be called when:
  //    1. User session start up tasks has completed.
  //    2. User session start up tasks has not completed, but user has start to
  //    search in launcher.
  //    3. In tests with fake search handler provided.
  void MaybeInitialize(
      ash::shortcut_ui::SearchHandler* fake_search_handler = nullptr);

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStartUpTaskCompleted() override;

 private:
  // When results are returned from `search_handler_`, re-calculates the
  // relevance score with tokenized string match on `description` and publishes
  // the most relevant results.
  void OnShortcutsSearchComplete(
      const std::u16string& query,
      std::vector<ash::shortcut_customization::mojom::SearchResultPtr>);

  const raw_ptr<Profile> profile_;
  bool has_initialized = false;

  // The |search_handler_| is managed by ShortcutsAppManager which is
  // implemented as a KeyedService, active for the lifetime of a logged-in user.
  raw_ptr<ash::shortcut_ui::SearchHandler> search_handler_ = nullptr;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<KeyboardShortcutProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_KEYBOARD_SHORTCUT_PROVIDER_H_
