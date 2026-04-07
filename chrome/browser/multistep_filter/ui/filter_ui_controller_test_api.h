// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

namespace multistep_filter {

class FilterUiControllerTestApi {
 public:
  explicit FilterUiControllerTestApi(FilterUiController& controller)
      : controller_(controller) {}

  const std::optional<UrlFilterSuggestion>& current_url_filter_suggestion() const {
    return controller_->current_url_filter_suggestion_;
  }

 private:
  const base::raw_ref<FilterUiController> controller_;
};

inline FilterUiControllerTestApi test_api(FilterUiController& controller) {
  return FilterUiControllerTestApi(controller);
}

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_UI_FILTER_UI_CONTROLLER_TEST_API_H_
