// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_PROMPT_SUGGESTION_H_
#define CHROME_BROWSER_GLIC_SELECTION_PROMPT_SUGGESTION_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/selection/suggestion.h"

namespace tabs {
class TabInterface;
}

namespace glic {

class PromptSuggestion : public ::selection::Suggestion {
 public:
  PromptSuggestion(tabs::TabInterface& tab,
                   std::u16string label,
                   std::string prompt);
  ~PromptSuggestion() override;

  // ::selection::Suggestion:
  const std::u16string& GetLabel() const override;
  void OnSuggestionPresented() override;
  void OnSuggestionExecuted() override;
  ::selection::mojom::ActionPtr GetAction() const override;

  const std::string& prompt() const { return prompt_; }

 private:
  const raw_ref<tabs::TabInterface> tab_;
  std::u16string label_;
  std::string prompt_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_PROMPT_SUGGESTION_H_
