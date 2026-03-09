// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

namespace multistep_filter {

// Manages the UI lifecycle and user interactions for multistep filter
// suggestions within a tab.
class FilterUiController : public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(FilterUiController);

  static FilterUiController* From(tabs::TabInterface* tab);

  explicit FilterUiController(tabs::TabInterface& tab);
  FilterUiController(const FilterUiController&) = delete;
  FilterUiController& operator=(const FilterUiController&) = delete;
  ~FilterUiController() override;

  // Callback for when a suggestion is generated.
  void OnSuggestionGenerated(std::optional<UrlFilterSuggestion> suggestion);

  // Returns a callback that handles the generation of a URL filter suggestion.
  base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
  GetSuggestionCallback();

  // Clears the current suggestion and hides the UI.
  void ClearSuggestion();

  // Applies the current suggestion by navigating to the suggested URL.
  void ApplySuggestion();

 protected:
  // Shows the UI for the given suggestion.
  virtual void ShowSuggestionUi(const UrlFilterSuggestion& suggestion);

  // Hides the suggestion UI.
  virtual void HideSuggestionUi();

  // Navigates the current tab to the given URL. Virtual for testing.
  virtual void NavigateTo(const GURL& url);

 private:
  ui::ScopedUnownedUserData<FilterUiController> scoped_unowned_user_data_;

  // The current suggestion that is displayed in the UI. Cached here so that
  // when the user accepts the suggestion, we have access to the details without
  // needing to pass them back from the UI layer.
  std::optional<UrlFilterSuggestion> current_url_filter_suggestion_;

  // Must be the last member variable to ensure that it is destroyed first,
  // invalidating all weak pointers before other members are destroyed.
  base::WeakPtrFactory<FilterUiController> weak_factory_{this};
};

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_
