// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

class PrefService;

namespace multistep_filter {

class MultistepFilterService;

class FilterUiControllerTestApi {
 public:
  explicit FilterUiControllerTestApi(FilterUiController& controller)
      : controller_(controller) {}

  const std::optional<FilterUiController::SuggestionState>& suggestion_state()
      const {
    return controller_->suggestion_state_;
  }

  void set_service(MultistepFilterService* service) {
    controller_->service_ = service;
  }

  void set_page_action_controller(
      page_actions::PageActionController* controller) {
    controller_->page_action_controller_ = controller;
    if (controller) {
      controller_->RegisterAsPageActionObserver(*controller);
    }
  }

  void set_pref_service(PrefService* service) {
    controller_->pref_service_ = service;
  }

  void set_favicon_service(favicon::FaviconService* service) {
    controller_->favicon_service_ = service;
  }

  // Exposes private SimpleMenuModel::Delegate overrides for verification.
  bool IsCommandIdChecked(int command_id) const {
    return controller_->IsCommandIdChecked(command_id);
  }

  bool IsCommandIdEnabled(int command_id) const {
    return controller_->IsCommandIdEnabled(command_id);
  }

  // Exposes private ExecuteCommand method to simulate menu/command clicks in
  // tests.
  void ExecuteCommand(int command_id, int event_flags) {
    controller_->ExecuteCommand(command_id, event_flags);
  }

  // Exposes private observer callbacks for unit test simulation.
  void OnPageActionAnchoredMessageShown(
      const page_actions::PageActionState& page_action) {
    controller_->OnPageActionAnchoredMessageShown(page_action);
  }

  void OnPageActionAnchoredMessageHidden(
      const page_actions::PageActionState& page_action) {
    controller_->OnPageActionAnchoredMessageHidden(page_action);
  }

 private:
  const base::raw_ref<FilterUiController> controller_;
};

inline FilterUiControllerTestApi test_api(FilterUiController& controller) {
  return FilterUiControllerTestApi(controller);
}

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_
