// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_SUGGESTER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"

namespace ash::input_method {

class SuggestionHandlerInterface;

class LongpressSuggester : public Suggester {
 public:
  explicit LongpressSuggester(SuggestionHandlerInterface* suggestion_handler);
  ~LongpressSuggester() override;

  virtual void Reset() {}

  // Suggester overrides:
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::AssistiveSuggestion>& suggestions,
      const std::optional<ime::SuggestionsTextContext>& context) override;
  bool HasSuggestions() override;
  std::vector<ime::AssistiveSuggestion> GetSuggestions() override;

 protected:
  const raw_ptr<SuggestionHandlerInterface, DanglingUntriaged>
      suggestion_handler_;
  std::optional<int> focused_context_id_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_SUGGESTER_H_
