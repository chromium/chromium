// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ABA_SEARCH_JOURNEY_CUE_TARGET_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ABA_SEARCH_JOURNEY_CUE_TARGET_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "ui/base/models/image_model.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {

// Contextual cue that offers to reopen the Search AI answer of an ABA search
// journey in the Contextual Tasks side panel, once the user returns to Tab A.
// The cue's text is generated locally, and clicking it restores the journey
// tracked by the tab's `SearchJourneyTabFeature`. It shows at most once per
// journey.
class AbaSearchJourneyCueTarget : public contextual_cueing::CueTarget {
 public:
  // Registers a target for `tab` with the tab's `ContextualCueingController`.
  static void Register(tabs::TabInterface& tab);

  explicit AbaSearchJourneyCueTarget(tabs::TabInterface& tab);
  AbaSearchJourneyCueTarget(const AbaSearchJourneyCueTarget&) = delete;
  AbaSearchJourneyCueTarget& operator=(const AbaSearchJourneyCueTarget&) =
      delete;
  ~AbaSearchJourneyCueTarget() override;

  // contextual_cueing::CueTarget:
  contextual_cueing::CueTargetType GetType() const override;
  bool RequiresModelExecution() const override;
  bool IsEligible() const override;
  void CheckEligibility(base::WeakPtr<content::WebContents> web_contents,
                        contextual_cueing::CueIntrusiveness intrusiveness,
                        EligibilityCallback callback) override;
  bool IsPageEligible(
      const page_content_annotations::PageContentAnnotationsResult& result,
      content::WebContents* active_web_contents) const override;
  void OnChipShown() override;
  void OnAnchoredMessageShown(
      page_actions::PageActionPriorityCategory priority) override;
  void OnAnchoredMessageClicked(contextual_cueing::CueActionData data) override;
  void OnEditPrompt(contextual_cueing::CueActionData data) override;
  ui::ImageModel GetAnchoredMessageIcon() const override;
  ui::ImageModel GetOmniboxChipIcon() const override;
  contextual_cueing::CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCue& cue,
      std::vector<tabs::TabHandle> tabs_to_show) const override;
  optimization_guide::proto::ContextualCueingSurface GetSurface()
      const override;

 private:
  // Evaluates the cue when the user returns to Tab A.
  void OnTabDidActivate(tabs::TabInterface* tab);

  // Records that the cue has shown for the tab's journey.
  void OnCueShown();

  // Builds the cue's text, unless the journey can no longer be restored.
  void GenerateContent(GenerateCallback callback);

  const raw_ref<tabs::TabInterface> tab_;
  base::CallbackListSubscription did_activate_subscription_;

  base::WeakPtrFactory<AbaSearchJourneyCueTarget> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ABA_SEARCH_JOURNEY_CUE_TARGET_H_
