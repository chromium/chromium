// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_SWITCH_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_SWITCH_H_

#include "base/functional/callback.h"
#include "ui/base/ime/ash/text_input_method.h"

namespace ash {
namespace input_method {

class AssistiveSuggesterSwitch {
 public:
  // Specifies the suggestions that are current enabled given the user's
  // context.
  struct EnabledSuggestions {
    bool emoji_suggestions = false;
    bool multi_word_suggestions = false;
    bool personal_info_suggestions = false;
    bool diacritic_suggestions = false;

    bool operator==(const EnabledSuggestions& rhs) const {
      return emoji_suggestions == rhs.emoji_suggestions &&
             multi_word_suggestions == rhs.multi_word_suggestions &&
             personal_info_suggestions == rhs.personal_info_suggestions &&
             diacritic_suggestions == rhs.diacritic_suggestions;
    }
  };

  AssistiveSuggesterSwitch() = default;
  virtual ~AssistiveSuggesterSwitch() = default;

  using FetchEnabledSuggestionsCallback =
      base::OnceCallback<void(const EnabledSuggestions&)>;

  // Gets the currently enabled suggestions given the current user context.
  virtual void FetchEnabledSuggestionsThen(
      FetchEnabledSuggestionsCallback callback,
      const TextInputMethod::InputContext& context) = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_SWITCH_H_
