// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_SWITCH_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_SWITCH_H_

#include "base/callback.h"

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
  };

  AssistiveSuggesterSwitch() = default;
  virtual ~AssistiveSuggesterSwitch() = default;

  // Are emoji suggestions allowed to be surfaced to the user?
  //
  // TODO(crbug/1146266): Deprecated, remove this method in favor of
  //     GetEnabledSuggestions.
  virtual bool IsEmojiSuggestionAllowed() = 0;

  // Are multi word suggestions allowed to be surfaced to the user?
  //
  // TODO(crbug/1146266): Deprecated, remove this method in favor of
  //     GetEnabledSuggestions.
  virtual bool IsMultiWordSuggestionAllowed() = 0;

  // Are personal info suggestions allowed to be surfaced to the user?
  //
  // TODO(crbug/1146266): Deprecated, remove this method in favor of
  //     GetEnabledSuggestions.
  virtual bool IsPersonalInfoSuggestionAllowed() = 0;

  using GetEnabledSuggestionsCallback =
      base::OnceCallback<void(const EnabledSuggestions&)>;

  // Gets the currently enabled suggestions given the current user context.
  virtual void GetEnabledSuggestions(
      GetEnabledSuggestionsCallback callback) = 0;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_SWITCH_H_
