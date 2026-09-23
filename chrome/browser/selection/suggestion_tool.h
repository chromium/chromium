// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_
#define CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/selection/suggestion.h"

namespace selection {

// Callback signature for suggestions retrieval.
using SuggestionsCallback = base::RepeatingCallback<
    void(std::vector<std::unique_ptr<Suggestion>> suggestions, bool complete)>;

// Interface for Chrome features to register as suggestion tools.
class SuggestionTool {
 public:
  virtual ~SuggestionTool() = default;

  virtual void RequestSuggestions(const AreaOfInterest& processed_area,
                                  SuggestionsCallback callback) = 0;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_SUGGESTION_TOOL_H_

