// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SUGGESTIONS_CONTEXTUAL_CUEING_HELPER_H_
#define CHROME_BROWSER_GLIC_SUGGESTIONS_CONTEXTUAL_CUEING_HELPER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_enums.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class OptimizationGuideKeyedService;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {
class GlicNudgeController;
}  // namespace glic

namespace glic {

class ContextualCueingService;
class ScopedNudgeDecisionRecorder;
struct CueingResult;

class ContextualCueingHelper : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(ContextualCueingHelper);

  // Creates a ContextualCueingHelper for `tab` if contextual cueing is
  // enabled. The returned helper is owned by the tab's TabFeatures. `tab`
  // must be non-null and must outlive the returned helper (the helper
  // registers itself on the tab's UnownedUserDataHost).
  static std::unique_ptr<ContextualCueingHelper> MaybeCreate(
      tabs::TabInterface* tab);

  // Returns the helper owned by `tab`'s TabFeatures, or nullptr if it was
  // not created.
  static ContextualCueingHelper* From(tabs::TabInterface* tab);

  ContextualCueingHelper(const ContextualCueingHelper&) = delete;
  ContextualCueingHelper& operator=(const ContextualCueingHelper&) = delete;
  ~ContextualCueingHelper() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;
  void OnFirstContentfulPaintInPrimaryMainFrame(
      base::TimeTicks presentation_time) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;

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

  glic::GlicNudgeController* GetGlicNudgeController();

 private:
  // All pointers must be non-null and must outlive `this`.
  ContextualCueingHelper(tabs::TabInterface* tab,
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
      base::expected<CueingResult, NudgeDecision> decision_result);

  enum class AutoOpenResult {
    kAutoOpened,
    kFallbackToNudge,
  };

  static AutoOpenResult RecordAutoOpenResult(GlicAutoOpenResult result);

  AutoOpenResult AutoOpenGlicSidePanel(
      const CueingResult& decision_result,
      ScopedNudgeDecisionRecorder* decision_recorder,
      bool is_pdf_candidate);

  bool IsBrowserBlockingNudges(ScopedNudgeDecisionRecorder* recorder);

  // When the last same doc navigation was committed.
  std::optional<base::TimeTicks> last_same_doc_navigation_committed_;

  bool has_first_contentful_paint_ = false;

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<ContextualCueingService> contextual_cueing_service_ = nullptr;

  ui::ScopedUnownedUserData<ContextualCueingHelper> scoped_unowned_user_data_;

  base::WeakPtrFactory<ContextualCueingHelper> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SUGGESTIONS_CONTEXTUAL_CUEING_HELPER_H_
