// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/static_selection_suggestion_tool.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/selection/prompt_suggestion.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"

namespace glic {

StaticSelectionSuggestionTool::StaticSelectionSuggestionTool(
    tabs::TabInterface& tab)
    : tab_(tab) {}

StaticSelectionSuggestionTool::~StaticSelectionSuggestionTool() =
    default;

StaticSelectionSuggestionTool::ToolId
StaticSelectionSuggestionTool::GetToolId() const {
  return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
}

void StaticSelectionSuggestionTool::RequestSuggestions(
    const ::selection::AreaOfInterest& processed_area,
    ::selection::SuggestionsCallback callback) {
  std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
  suggestions.push_back(
      std::make_unique<PromptSuggestion>(*tab_, u"Ask Gemini"));
  std::move(callback).Run(std::move(suggestions), /*complete=*/true);
}

std::unique_ptr<::selection::Suggestion>
StaticSelectionSuggestionTool::CreateSuggestion(
    const optimization_guide::proto::SmartSelectionSuggestion&
        server_suggestion) {
  if (server_suggestion.label().empty()) {
    return nullptr;
  }
  return std::make_unique<PromptSuggestion>(
      *tab_, base::UTF8ToUTF16(server_suggestion.label()),
      server_suggestion.label());
}

}  // namespace glic

