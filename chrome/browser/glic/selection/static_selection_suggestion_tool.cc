// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/static_selection_suggestion_tool.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/glic/selection/prompt_suggestion.h"

namespace glic {

StaticSelectionSuggestionTool::StaticSelectionSuggestionTool(
    tabs::TabInterface& tab)
    : tab_(tab) {}

StaticSelectionSuggestionTool::~StaticSelectionSuggestionTool() =
    default;

void StaticSelectionSuggestionTool::RequestSuggestions(
    const ::selection::AreaOfInterest& processed_area,
    ::selection::SuggestionsCallback callback) {
  std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
  suggestions.push_back(std::make_unique<PromptSuggestion>(
      *tab_, u"Explain", "Explain the selection in a few sentences."));
  suggestions.push_back(std::make_unique<PromptSuggestion>(
      *tab_, u"Summarize", "Summarize the selection in a few sentences."));
  suggestions.push_back(std::make_unique<PromptSuggestion>(
      *tab_, u"Create Image",
      "Create a cartoon styled image from the selection."));
  std::move(callback).Run(std::move(suggestions), /*complete=*/true);
}

}  // namespace glic

