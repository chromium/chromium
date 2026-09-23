// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_STATIC_SELECTION_SUGGESTION_TOOL_H_
#define CHROME_BROWSER_GLIC_SELECTION_STATIC_SELECTION_SUGGESTION_TOOL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/selection/suggestion_tool.h"

namespace tabs {
class TabInterface;
}

namespace glic {

class StaticSelectionSuggestionTool
    : public ::selection::SuggestionTool {
 public:
  explicit StaticSelectionSuggestionTool(tabs::TabInterface& tab);
  ~StaticSelectionSuggestionTool() override;

  // ::selection::SuggestionTool:
  void RequestSuggestions(const ::selection::AreaOfInterest& processed_area,
                          ::selection::SuggestionsCallback callback) override;

 private:
  const raw_ref<tabs::TabInterface> tab_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_STATIC_SELECTION_SUGGESTION_TOOL_H_

