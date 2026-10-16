// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_SUGGESTION_TOOL_H_
#define CHROME_BROWSER_LENS_LENS_SUGGESTION_TOOL_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/selection/suggestion_tool.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace lens {

BASE_DECLARE_FEATURE(kLensSelectionSuggestions);

// Suggestion tool that opens the Lens overlay with the selected area of
// interest.
class LensSuggestionTool : public ::selection::SuggestionTool {
 public:
  explicit LensSuggestionTool(tabs::TabInterface& tab);
  LensSuggestionTool(const LensSuggestionTool&) = delete;
  LensSuggestionTool& operator=(const LensSuggestionTool&) = delete;
  ~LensSuggestionTool() override;

  // ::selection::SuggestionTool:
  ToolId GetToolId() const override;
  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override;

 private:
  const raw_ref<tabs::TabInterface> tab_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_SUGGESTION_TOOL_H_
