// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/quick_answers_tool.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/selection/explain_suggestion.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/proto/features/quick_answers.pb.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/quick_answers_metadata.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
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

DEFINE_USER_DATA(QuickAnswersTool);

// static
QuickAnswersTool* QuickAnswersTool::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
QuickAnswersTool* QuickAnswersTool::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  if (!tab_web_contents) {
    return nullptr;
  }
  return From(tabs::TabInterface::MaybeGetFromContents(tab_web_contents));
}

QuickAnswersTool::QuickAnswersTool(tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      tab_(tab),
      optimization_guide_(OptimizationGuideKeyedServiceFactory::GetForProfile(
          tab.GetProfile())),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  if (optimization_guide_) {
    optimization_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::OptimizationType::QUICK_ANSWERS});
  }
  tab_discard_subscription_ =
      tab_->RegisterWillDiscardContents(base::BindRepeating(
          &QuickAnswersTool::WillDiscardContents, base::Unretained(this)));
  CheckQuickAnswersEligibility(
      web_contents() ? web_contents()->GetLastCommittedURL() : GURL());
}

QuickAnswersTool::~QuickAnswersTool() {
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(&tab_.get())) {
    suggestion_service->UnregisterTool(this);
  }
}

std::unique_ptr<::selection::Suggestion> QuickAnswersTool::CreateSuggestion(
    const ::selection::AreaOfInterest& processed_area) {
  if (!IsEligible()) {
    return nullptr;
  }
  if (processed_area.selected_text.has_value() &&
      !processed_area.selected_text->empty() &&
      processed_area.text_surrounding_selection.has_value() &&
      !processed_area.text_surrounding_selection->empty()) {
    return std::make_unique<ExplainSuggestion>(
        *tab_, BuildQuickAnswersRequest(*tab_, processed_area));
  }
  return nullptr;
}

QuickAnswersTool::ToolId QuickAnswersTool::GetToolId() const {
  return optimization_guide::proto::SMART_SELECTION_TOOL_QUICK_ANSWERS;
}

std::unique_ptr<::selection::Suggestion> QuickAnswersTool::CreateSuggestion(
    const ::selection::AreaOfInterest& processed_area,
    const optimization_guide::proto::SmartSelectionSuggestion&
        server_suggestion) {
  return CreateSuggestion(processed_area);
}

void QuickAnswersTool::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_handle->IsSameDocument() &&
      navigation_handle->GetPreviousPrimaryMainFrameURL().EqualsIgnoringRef(
          navigation_handle->GetURL())) {
    return;
  }

  CheckQuickAnswersEligibility(navigation_handle->GetURL());
}

void QuickAnswersTool::WillDiscardContents(tabs::TabInterface* tab,
                                           content::WebContents* old_contents,
                                           content::WebContents* new_contents) {
  Observe(new_contents);
  CheckQuickAnswersEligibility(
      new_contents ? new_contents->GetLastCommittedURL() : GURL());
}

void QuickAnswersTool::CheckQuickAnswersEligibility(const GURL& url) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  SetEligible(false);
  if (!optimization_guide_ || !url.is_valid()) {
    return;
  }

  optimization_guide_->CanApplyOptimization(
      url, optimization_guide::proto::OptimizationType::QUICK_ANSWERS,
      base::BindOnce(&QuickAnswersTool::OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

void QuickAnswersTool::OnOptimizationGuideDecision(
    const GURL& url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (!web_contents() ||
      !url.EqualsIgnoringRef(web_contents()->GetLastCommittedURL())) {
    return;
  }

  bool is_eligible = false;
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue) {
    auto quick_answers_metadata =
        metadata
            .ParsedMetadata<optimization_guide::proto::QuickAnswersMetadata>();
    if (quick_answers_metadata) {
      is_eligible = quick_answers_metadata->is_eligible();
    }
  }
  SetEligible(is_eligible);
}

void QuickAnswersTool::SetEligible(bool is_eligible) {
  is_eligible_ = is_eligible;
  UpdateToolRegistration();
}

void QuickAnswersTool::UpdateToolRegistration() {
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(&tab_.get())) {
    suggestion_service->UnregisterTool(this);
    if (IsEligible()) {
      suggestion_service->RegisterTool(this);
    }
  }
}

}  // namespace glic
