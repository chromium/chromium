// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class OptimizationGuideKeyedService;

namespace tabs {
class GlicNudgeController;
}  // namespace tabs

namespace contextual_cueing {

class ContextualCueingService;
class ScopedNudgeDecisionRecorder;

class ContextualCueingHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContextualCueingHelper> {
 public:
  // Creates ContextualCueingHelper and attaches it the `web_contents` if
  // contextual cueing is enabled.
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ContextualCueingHelper(const ContextualCueingHelper&) = delete;
  ContextualCueingHelper& operator=(const ContextualCueingHelper&) = delete;
  ~ContextualCueingHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;
  void OnFirstContentfulPaintInPrimaryMainFrame() override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // Returns when the last primary main frame navigation was committed if the
  // navigation was a same document navigation.
  std::optional<base::TimeTicks> last_same_doc_navigation_committed() const {
    return last_same_doc_navigation_committed_;
  }

  // Returns whether the last primary main frame navigation that was committed
  // has already past FCP.
  bool has_first_contentful_paint() const {
    return has_first_contentful_paint_;
  }

  tabs::GlicNudgeController* GetGlicNudgeController();

 private:
  ContextualCueingHelper(content::WebContents* contents,
                         OptimizationGuideKeyedService* ogks,
                         ContextualCueingService* ccs);

  // Called when optimization guide metadata is received.
  void OnOptimizationGuideCueingMetadata(
      base::TimeTicks document_available_time,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  void OnCueingDecision(
      std::unique_ptr<ScopedNudgeDecisionRecorder> decision_recorder,
      base::TimeTicks document_available_time,
      base::expected<std::string, NudgeDecision> decision_result);

  bool IsBrowserBlockingNudges(ScopedNudgeDecisionRecorder* recorder);

  // When the last same doc navigation was committed.
  std::optional<base::TimeTicks> last_same_doc_navigation_committed_;

  bool has_first_contentful_paint_ = false;

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<ContextualCueingService> contextual_cueing_service_ = nullptr;

  base::WeakPtrFactory<ContextualCueingHelper> weak_ptr_factory_{this};

  friend WebContentsUserData<ContextualCueingHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_HELPER_H_
