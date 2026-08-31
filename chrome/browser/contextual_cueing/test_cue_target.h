// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_TEST_CUE_TARGET_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_TEST_CUE_TARGET_H_

#include "chrome/browser/contextual_cueing/cue_target.h"

namespace contextual_cueing {

class TestCueTarget : public CueTarget {
 public:
  bool eligible = true;
  bool requires_model_execution = false;
  std::optional<CueIntrusiveness> eligible_intrusiveness;
  std::set<CueIntrusiveness> supported_intrusiveness = {
      CueIntrusiveness::kLoud, CueIntrusiveness::kQuiet};
  std::optional<optimization_guide::proto::ContextualCue> generate_result;
  bool page_eligible = true;
  CueActionData click_data = std::monostate();
  CueActionData edit_prompt_data = std::monostate();
  bool chip_shown = false;
  bool chip_clicked = false;
  std::optional<page_actions::PageActionPriorityCategory>
      anchored_message_shown_priority;

  TestCueTarget();
  ~TestCueTarget() override;

  bool HasClickData() const;

  // CueTarget:
  CueTargetType GetType() const override;
  bool RequiresModelExecution() const override;
  bool IsEligible() const override;
  void CheckEligibility(base::WeakPtr<content::WebContents> web_contents,
                        CueIntrusiveness intrusiveness,
                        EligibilityCallback callback) override;
  bool IsPageEligible(
      const page_content_annotations::PageContentAnnotationsResult& result,
      content::WebContents* active_web_contents) const override;
  void OnChipShown() override;
  void OnChipClicked() override;
  void OnAnchoredMessageShown(
      page_actions::PageActionPriorityCategory priority) override;
  void OnAnchoredMessageClicked(CueActionData data) override;
  void OnEditPrompt(CueActionData data) override;
  ui::ImageModel GetAnchoredMessageIcon() const override;
  ui::ImageModel GetOmniboxChipIcon() const override;
  CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCue& cue,
      std::vector<tabs::TabHandle> tabs_to_show) const override;
  optimization_guide::proto::ContextualCueingSurface GetSurface()
      const override;

 protected:
  bool SupportsIntrusivenessImpl(CueIntrusiveness intrusiveness) const override;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_TEST_CUE_TARGET_H_
