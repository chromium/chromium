// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/quick_answers_tool.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/selection/explain_suggestion.h"
#include "chrome/browser/selection/suggestion.h"
#include "components/optimization_guide/proto/features/quick_answers.pb.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace glic {

namespace {

optimization_guide::proto::QuickAnswersRequest BuildQuickAnswersRequest(
    tabs::TabInterface& tab,
    const ::selection::AreaOfInterest& processed_area) {
  optimization_guide::proto::QuickAnswersRequest request;
  request.set_selected_text(base::UTF16ToUTF8(*processed_area.selected_text));
  request.set_surrounding_text(
      base::UTF16ToUTF8(*processed_area.text_surrounding_selection));

  if (content::WebContents* web_contents = tab.GetContents()) {
    request.set_page_title(base::UTF16ToUTF8(web_contents->GetTitle()));
    request.set_page_url(web_contents->GetLastCommittedURL().spec());
  }

  if (g_browser_process) {
    request.set_user_locale(g_browser_process->GetApplicationLocale());
  }

  return request;
}

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
  if (processed_area.selected_text.has_value() &&
      !processed_area.selected_text->empty() &&
      processed_area.text_surrounding_selection.has_value() &&
      !processed_area.text_surrounding_selection->empty()) {
    suggestions.push_back(std::make_unique<ExplainSuggestion>(
        *tab_, BuildQuickAnswersRequest(*tab_, processed_area)));
  }
  std::move(callback).Run(std::move(suggestions), /*complete=*/true);
}

}  // namespace glic
