// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion_tool.h"

#include <memory>

#include "chrome/browser/selection/suggestion.h"

namespace selection {

bool SuggestionTool::SupportsServerSuggestions() const {
  return false;
}

std::unique_ptr<Suggestion> SuggestionTool::CreateSuggestion(
    const optimization_guide::proto::SmartSelectionSuggestion&) {
  return nullptr;
}

}  // namespace selection
