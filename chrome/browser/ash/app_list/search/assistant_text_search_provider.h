// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ASSISTANT_TEXT_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ASSISTANT_TEXT_SEARCH_PROVIDER_H_

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

namespace app_list {

// A search provider implementation serving results from Assistant.
// This is currently only used to provide a single search result that runs an
// Assistant query of the search text. This search result does not go
// through normal ranking procedures, but is instead appended to an existing
// list of search results.
class AssistantTextSearchProvider : public SearchProvider,
                                    public ash::AssistantControllerObserver,
                                    public ash::AssistantStateObserver {
 public:
  AssistantTextSearchProvider();
  AssistantTextSearchProvider(const AssistantTextSearchProvider&) = delete;
  AssistantTextSearchProvider& operator=(const AssistantTextSearchProvider&) =
      delete;
  ~AssistantTextSearchProvider() override;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;

 private:
  // SearchProvider:
  ash::AppListSearchResultType ResultType() const override;

  // ash::AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // ash::AssistantStateObserver:
  void OnAssistantFeatureAllowedChanged(
      ash::assistant::AssistantAllowedState allowed_state) override;
  void OnAssistantSettingsEnabled(bool enabled) override;

  // Invoke to update results based on current state.
  void UpdateResults();

  std::u16string query_;

  base::ScopedObservation<ash::AssistantController,
                          ash::AssistantControllerObserver>
      assistant_controller_observation_{this};

  base::ScopedObservation<ash::AssistantStateBase, ash::AssistantStateObserver>
      assistant_state_observation_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ASSISTANT_TEXT_SEARCH_PROVIDER_H_
