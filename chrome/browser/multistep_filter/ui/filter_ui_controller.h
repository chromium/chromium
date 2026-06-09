// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/favicon_base/favicon_types.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/menus/simple_menu_model.h"

class GURL;

namespace tabs {
class TabInterface;
}

namespace page_actions {
class PageActionController;
}

namespace favicon {
class FaviconService;
}

namespace multistep_filter {

namespace internal {
inline constexpr int kDismissCommand = 1;
inline constexpr int kSettingsCommand = 2;
}  // namespace internal

class FilterUiControllerTestApi;
class MultistepFilterLogRouter;
class MultistepFilterService;

// Manages the UI lifecycle and user interactions for multistep filter
// suggestions within a tab.
class FilterUiController : public tabs::ContentsObservingTabFeature,
                           public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_USER_DATA(FilterUiController);

  // The user's decision upon interacting with the suggestion.
  enum class SuggestionUserDecision {
    kAccepted,
    kDismissed,
    kIgnored,
  };

  static FilterUiController* From(tabs::TabInterface* tab);

  explicit FilterUiController(tabs::TabInterface& tab);
  FilterUiController(const FilterUiController&) = delete;
  FilterUiController& operator=(const FilterUiController&) = delete;
  ~FilterUiController() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Callback for when a suggestion is generated.
  virtual void OnSuggestionGenerated(
      std::optional<UrlFilterSuggestion> suggestion);

  // Clears the current suggestion, hides the UI, and logs the action.
  virtual void ClearSuggestion(SuggestionUserDecision decision);

  // Applies the current suggestion by navigating to the suggested URL.
  virtual void ApplySuggestion();

  // Handles the action invocation from the location bar or bubble.
  virtual void OnActionInvoked();

 protected:
  // Navigates the current tab to the given URL. Virtual for testing.
  virtual void NavigateTo(const GURL& url);

 private:
  friend class FilterUiControllerTestApi;

  // Handles the dismissal of the suggestion.
  void DismissSuggestion();

  // Opens the settings page.
  void OpenSettings();

  // Shows the cue for the given suggestion.
  void ShowCue(const UrlFilterSuggestion& suggestion);

  // Clears the cue UI.
  void ClearCue();

  // Callback for when the favicon image is available.
  void OnFaviconAvailable(UrlFilterSuggestion suggestion,
                          const favicon_base::FaviconImageResult& result);

  // Helper variable to scope tab instance unowned user data ownership.
  ui::ScopedUnownedUserData<FilterUiController> scoped_unowned_user_data_;

  // The current suggestion that is displayed in the UI. Cached here so that
  // when the user accepts the suggestion, we have access to the details without
  // needing to pass them back from the UI layer.
  std::optional<UrlFilterSuggestion> current_url_filter_suggestion_;

  // Router for logging filter events.
  raw_ptr<MultistepFilterLogRouter> log_router_ = nullptr;

  // Service for managing filters.
  raw_ptr<MultistepFilterService> service_ = nullptr;

  // Controller for the page action.
  raw_ptr<page_actions::PageActionController> page_action_controller_ = nullptr;

  // Service for fetching favicons.
  raw_ptr<favicon::FaviconService> favicon_service_ = nullptr;

  // Tracker for favicon fetch requests.
  base::CancelableTaskTracker favicon_task_tracker_;

  // Factory for dismissal callbacks. Must be the last member variable to
  // ensure that it is destroyed first, invalidating all weak pointers before
  // other members are destroyed.
  base::WeakPtrFactory<FilterUiController> dismissal_weak_factory_{this};
};

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_
