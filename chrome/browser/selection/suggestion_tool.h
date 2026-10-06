// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_
#define CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/selection/suggestion.h"

namespace optimization_guide::proto {
class SmartSelectionSuggestion;
enum SmartSelectionToolId : int;
}  // namespace optimization_guide::proto

namespace selection {

// Callback signature for suggestions retrieval.
using SuggestionsCallback = base::RepeatingCallback<
    void(std::vector<std::unique_ptr<Suggestion>> suggestions, bool complete)>;

// Interface for Chrome features to register as suggestion tools.
class SuggestionTool {
 public:
  using ToolId = optimization_guide::proto::SmartSelectionToolId;

  virtual ~SuggestionTool() = default;

  virtual ToolId GetToolId() const = 0;

  // Runs `callback` with suggestions that the tool proposed for the
  // `processed_area`.
  virtual void RequestSuggestions(const AreaOfInterest& processed_area,
                                  SuggestionsCallback callback) = 0;

  // Returns whether the server may generate suggestions for this tool. If
  // `true`, this tool's `ToolId` is included in the supported tools server
  // request for suggestion generation.
  virtual bool SupportsServerSuggestions() const;

  // Turns the `server_suggestion` into a `Suggestion`. That is, it acts as a
  // factory function for suggestions that used this tool.
  virtual std::unique_ptr<Suggestion> CreateSuggestion(
      const AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion);
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_
