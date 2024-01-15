// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_CLIENT_FILTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_CLIENT_FILTER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/input_method/assistive_input_denylist.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/get_current_window_properties.h"

namespace ash {
namespace input_method {

class AssistiveSuggesterClientFilter : public AssistiveSuggesterSwitch {
 public:
  using GetUrlCallback =
      base::RepeatingCallback<void(GetFocusedTabUrlCallback)>;

  using GetFocusedWindowPropertiesCallback =
      base::RepeatingCallback<WindowProperties(void)>;

  AssistiveSuggesterClientFilter(
      GetUrlCallback get_url,
      GetFocusedWindowPropertiesCallback get_window_properties);

  ~AssistiveSuggesterClientFilter() override;

  // AssistiveSuggesterSwitch overrides
  void FetchEnabledSuggestionsThen(
      FetchEnabledSuggestionsCallback callback,
      const TextInputMethod::InputContext& context) override;

 private:
  void ReturnEnabledSuggestions(
      AssistiveSuggesterSwitch::FetchEnabledSuggestionsCallback callback,
      WindowProperties window_properties,
      const TextInputMethod::InputContext& context,
      const std::optional<GURL>& current_url);

  // Used to fetch the url from the current browser instance.
  GetUrlCallback get_url_;
  GetFocusedWindowPropertiesCallback get_window_properties_;
  AssistiveInputDenylist denylist_;
  base::WeakPtrFactory<AssistiveSuggesterClientFilter> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_CLIENT_FILTER_H_
