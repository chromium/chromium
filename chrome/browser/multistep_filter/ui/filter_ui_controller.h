// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class GURL;

namespace tabs {
class TabInterface;
}

namespace multistep_filter {

class FilterUiControllerTestApi;

// Manages the UI lifecycle and user interactions for multistep filter
// suggestions within a tab.
class FilterUiController : public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(FilterUiController);

  // Information needed to show a suggestion toast.
  struct SuggestionUiData {
    SuggestionUiData(ToastId toast_id,
                     std::vector<std::u16string> replacement_params);
    SuggestionUiData(const SuggestionUiData&);
    SuggestionUiData& operator=(const SuggestionUiData&);
    ~SuggestionUiData();

    friend bool operator==(const SuggestionUiData&,
                           const SuggestionUiData&) = default;

    // The ID of the toast to show.
    ToastId toast_id;

    // The string parameters to replace in the toast body text.
    std::vector<std::u16string> replacement_params;
  };

  static FilterUiController* From(tabs::TabInterface* tab);

  explicit FilterUiController(tabs::TabInterface& tab);
  FilterUiController(const FilterUiController&) = delete;
  FilterUiController& operator=(const FilterUiController&) = delete;
  ~FilterUiController() override;

  // Callback for when a suggestion is generated.
  virtual void OnSuggestionGenerated(
      std::optional<UrlFilterSuggestion> suggestion);

  // Clears the current suggestion and hides the UI.
  virtual void ClearSuggestion();

  // Applies the current suggestion by navigating to the suggested URL.
  virtual void ApplySuggestion();

  // Returns true if suggestions should be suppressed for the given URL.
  virtual bool ShouldSuppressSuggestions(const GURL& url) const;

  // Returns the UI data for the given `suggestion` and `time`.
  SuggestionUiData GetSuggestionUiData(const UrlFilterSuggestion& suggestion,
                                       base::Time now) const;

 protected:
  // Shows the UI for the given suggestion.
  virtual void ShowSuggestionUi(const UrlFilterSuggestion& suggestion);

  // Navigates the current tab to the given URL. Virtual for testing.
  virtual void NavigateTo(const GURL& url);

  // Returns the callback to be executed when the suggestion UI is dismissed.
  base::OnceClosure GetOnDismissedCallback(const GURL& url);

 private:
  friend class FilterUiControllerTestApi;

  void OnSuggestionDismissed(const GURL& url);

  ui::ScopedUnownedUserData<FilterUiController> scoped_unowned_user_data_;

  // The current suggestion that is displayed in the UI. Cached here so that
  // when the user accepts the suggestion, we have access to the details without
  // needing to pass them back from the UI layer.
  std::optional<UrlFilterSuggestion> current_url_filter_suggestion_;

  // Hosts for which the user has dismissed a suggestion in the current tab.
  // TODO (crbug.com/495396112): Identify if dismissed hosts should be persisted
  // or shared across tabs.
  base::flat_set<std::string> dismissed_hosts_;

  // Factory for dismissal callbacks. Must be the last member variable to
  // ensure that it is destroyed first, invalidating all weak pointers before
  // other members are destroyed.
  base::WeakPtrFactory<FilterUiController> dismissal_weak_factory_{this};
};

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_H_
