// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_suggestion_tool.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/selection/selection_suggestion.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"

namespace glic {

SelectionSuggestionTool::SelectionSuggestionTool(tabs::TabInterface& tab)
    : tab_(tab) {
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(&tab_.get())) {
    suggestion_service->RegisterTool(this);
  }
}

SelectionSuggestionTool::~SelectionSuggestionTool() {
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(&tab_.get())) {
    suggestion_service->UnregisterTool(this);
  }
}

SelectionSuggestionTool::ToolId SelectionSuggestionTool::GetToolId() const {
  return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
}

void SelectionSuggestionTool::RequestSuggestions(
    const ::selection::AreaOfInterest& processed_area,
    ::selection::SuggestionsCallback callback) {
  std::move(callback).Run({}, /*complete=*/true);
}

bool SelectionSuggestionTool::SupportsServerSuggestions() const {
  return true;
}

std::unique_ptr<::selection::Suggestion>
SelectionSuggestionTool::CreateSuggestion(
    const ::selection::AreaOfInterest& processed_area,
    const optimization_guide::proto::SmartSelectionSuggestion&
        server_suggestion) {
  if (server_suggestion.label().empty()) {
    return nullptr;
  }
  return std::make_unique<SelectionSuggestion>(
      *tab_, base::UTF8ToUTF16(server_suggestion.label()),
      server_suggestion.label());
}

}  // namespace glic
