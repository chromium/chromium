// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_QUICK_ANSWERS_TOOL_H_
#define CHROME_BROWSER_GLIC_SELECTION_QUICK_ANSWERS_TOOL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/selection/suggestion_tool.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace tabs {
class TabInterface;
}

namespace glic {

class QuickAnswersTool : public ::selection::SuggestionTool,
                         public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(QuickAnswersTool);

  explicit QuickAnswersTool(tabs::TabInterface& tab);
  QuickAnswersTool(const QuickAnswersTool&) = delete;
  QuickAnswersTool& operator=(const QuickAnswersTool&) = delete;
  ~QuickAnswersTool() override;

  static QuickAnswersTool* From(tabs::TabInterface* tab);
  static QuickAnswersTool* FromTabWebContents(
      content::WebContents* tab_web_contents);

  bool IsEligible() const { return is_eligible_; }

  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area);

  // ::selection::SuggestionTool:
  ToolId GetToolId() const override;
  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);
  void CheckQuickAnswersEligibility(const GURL& url);
  void OnOptimizationGuideDecision(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);
  void SetEligible(bool is_eligible);
  void UpdateToolRegistration();

  const raw_ref<tabs::TabInterface> tab_;
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_;
  bool is_eligible_ = false;

  base::CallbackListSubscription tab_discard_subscription_;
  ui::ScopedUnownedUserData<QuickAnswersTool> scoped_unowned_user_data_;

  base::WeakPtrFactory<QuickAnswersTool> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_QUICK_ANSWERS_TOOL_H_
