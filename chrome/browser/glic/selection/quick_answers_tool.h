// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_QUICK_ANSWERS_TOOL_H_
#define CHROME_BROWSER_GLIC_SELECTION_QUICK_ANSWERS_TOOL_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/selection/suggestion_tool.h"

namespace tabs {
class TabInterface;
}

namespace glic {

class QuickAnswersTool : public ::selection::SuggestionTool {
 public:
  explicit QuickAnswersTool(tabs::TabInterface& tab);
  ~QuickAnswersTool() override;

  static std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      tabs::TabInterface& tab,
      const ::selection::AreaOfInterest& processed_area);

  // ::selection::SuggestionTool:
  ToolId GetToolId() const override;
  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override;

 private:
  const raw_ref<tabs::TabInterface> tab_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_QUICK_ANSWERS_TOOL_H_
