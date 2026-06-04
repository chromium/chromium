// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "components/favicon_base/favicon_types.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace multistep_filter {

class MultistepFilterService;

class FilterUiControllerTestApi {
 public:
  explicit FilterUiControllerTestApi(FilterUiController& controller)
      : controller_(controller) {}

  const std::optional<UrlFilterSuggestion>& current_url_filter_suggestion() const {
    return controller_->current_url_filter_suggestion_;
  }

  void set_service(MultistepFilterService* service) {
    controller_->service_ = service;
  }

  void set_page_action_controller(
      page_actions::PageActionController* controller) {
    controller_->page_action_controller_ = controller;
  }

  void set_favicon_service(favicon::FaviconService* service) {
    controller_->favicon_service_ = service;
  }

  // Exposes the private OnFaviconAvailable method to simulate asynchronous
  // favicon fetch returns in unit tests.
  void OnFaviconAvailable(UrlFilterSuggestion suggestion,
                          const favicon_base::FaviconImageResult& result) {
    controller_->OnFaviconAvailable(suggestion, result);
  }

 private:
  const base::raw_ref<FilterUiController> controller_;
};

inline FilterUiControllerTestApi test_api(FilterUiController& controller) {
  return FilterUiControllerTestApi(controller);
}

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_
