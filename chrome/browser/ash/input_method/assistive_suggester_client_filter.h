// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_CLIENT_FILTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_CLIENT_FILTER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"

namespace ash {
namespace input_method {

class AssistiveSuggesterClientFilter : public AssistiveSuggesterSwitch {
 public:
  AssistiveSuggesterClientFilter();
  ~AssistiveSuggesterClientFilter() override;

  // AssistiveSuggesterDelegate overrides
  bool IsEmojiSuggestionAllowed() override;
  bool IsMultiWordSuggestionAllowed() override;
  bool IsPersonalInfoSuggestionAllowed() override;
  void GetEnabledSuggestions(GetEnabledSuggestionsCallback callback) override;

 private:
  base::WeakPtrFactory<AssistiveSuggesterClientFilter> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_CLIENT_FILTER_H_
