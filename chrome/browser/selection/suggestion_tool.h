// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_
#define CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_

#include <memory>

#include "chrome/browser/selection/suggestion.h"

namespace optimization_guide::proto {
class SmartSelectionSuggestion;
enum SmartSelectionToolId : int;
}  // namespace optimization_guide::proto

namespace selection {

// Interface for Chrome features to register as suggestion tools.
class SuggestionTool {
 public:
  using ToolId = optimization_guide::proto::SmartSelectionToolId;

  virtual ~SuggestionTool() = default;

  virtual ToolId GetToolId() const = 0;

  // Turns the `server_suggestion` into a `Suggestion`. That is, it acts as a
  // factory function for suggestions that used this tool.
  virtual std::unique_ptr<Suggestion> CreateSuggestion(
      const AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) = 0;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_
