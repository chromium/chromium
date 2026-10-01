// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/quick_answers_tool.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/glic/selection/explain_suggestion.h"
#include "chrome/browser/selection/suggestion.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"

namespace glic {

QuickAnswersTool::QuickAnswersTool(tabs::TabInterface& tab) : tab_(tab) {}

QuickAnswersTool::~QuickAnswersTool() = default;

QuickAnswersTool::ToolId QuickAnswersTool::GetToolId() const {
  return optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED;
}

void QuickAnswersTool::RequestSuggestions(
    const ::selection::AreaOfInterest& processed_area,
    ::selection::SuggestionsCallback callback) {
  std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
  suggestions.push_back(std::make_unique<ExplainSuggestion>(*tab_));
  std::move(callback).Run(std::move(suggestions), /*complete=*/true);
}

}  // namespace glic
