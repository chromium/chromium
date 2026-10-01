// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/quick_answers_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/browser/selection/suggestion.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"

namespace glic {
namespace {

class ExplainSuggestion : public ::selection::Suggestion {
 public:
  ExplainSuggestion() = default;
  ~ExplainSuggestion() override = default;

  // ::selection::Suggestion:
  const std::u16string& GetLabel() const override { return label_; }
  void OnSuggestionPresented() override {}
  void OnSuggestionExecuted() override {}
  ::selection::mojom::ActionPtr GetAction() const override {
    return ::selection::mojom::Action::NewInlineFulfillment(
        ::selection::mojom::InlineFulfillment::New("explain_fulfillment.js"));
  }

 private:
  const std::u16string label_ = u"Explain";
};

}  // namespace

QuickAnswersTool::QuickAnswersTool(tabs::TabInterface& tab) : tab_(tab) {}

QuickAnswersTool::~QuickAnswersTool() = default;

QuickAnswersTool::ToolId QuickAnswersTool::GetToolId() const {
  return optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED;
}

void QuickAnswersTool::RequestSuggestions(
    const ::selection::AreaOfInterest& processed_area,
    ::selection::SuggestionsCallback callback) {
  std::vector<std::unique_ptr<::selection::Suggestion>> suggestions;
  suggestions.push_back(std::make_unique<ExplainSuggestion>());
  std::move(callback).Run(std::move(suggestions), /*complete=*/true);
}

std::unique_ptr<::selection::Suggestion> QuickAnswersTool::CreateSuggestion(
    const optimization_guide::proto::SmartSelectionSuggestion&
        server_suggestion) {
  return nullptr;
}

}  // namespace glic
