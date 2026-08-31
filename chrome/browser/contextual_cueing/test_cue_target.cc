// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/test_cue_target.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "content/public/browser/web_contents.h"

namespace contextual_cueing {

TestCueTarget::TestCueTarget() = default;
TestCueTarget::~TestCueTarget() = default;

bool TestCueTarget::HasClickData() const {
  return !std::holds_alternative<std::monostate>(click_data);
}

CueTargetType TestCueTarget::GetType() const {
  return CueTargetType::kTestSource;
}

bool TestCueTarget::RequiresModelExecution() const {
  return requires_model_execution || !generate_result.has_value();
}

bool TestCueTarget::SupportsIntrusivenessImpl(
    CueIntrusiveness intrusiveness) const {
  return supported_intrusiveness.contains(intrusiveness);
}

bool TestCueTarget::IsEligible() const {
  return eligible;
}

void TestCueTarget::CheckEligibility(
    base::WeakPtr<content::WebContents> web_contents,
    CueIntrusiveness intrusiveness,
    EligibilityCallback callback) {
  if (eligible && (!eligible_intrusiveness.has_value() ||
                   eligible_intrusiveness == intrusiveness)) {
    ContentGenerator generator;
    if (generate_result.has_value()) {
      generator = base::BindOnce(
          [](std::optional<optimization_guide::proto::ContextualCue> result,
             GenerateCallback cb) { std::move(cb).Run(std::move(result)); },
          generate_result);
    }
    std::move(callback).Run(true, std::move(generator));
  } else {
    std::move(callback).Run(false, ContentGenerator());
  }
}

bool TestCueTarget::IsPageEligible(
    const page_content_annotations::PageContentAnnotationsResult& result,
    content::WebContents* active_web_contents) const {
  return page_eligible;
}

void TestCueTarget::OnChipShown() {
  chip_shown = true;
}

void TestCueTarget::OnChipClicked() {
  chip_clicked = true;
}

void TestCueTarget::OnAnchoredMessageShown(
    page_actions::PageActionPriorityCategory priority) {
  anchored_message_shown_priority = priority;
}

void TestCueTarget::OnAnchoredMessageClicked(CueActionData data) {
  click_data = std::move(data);
}

void TestCueTarget::OnEditPrompt(CueActionData data) {
  edit_prompt_data = std::move(data);
}

ui::ImageModel TestCueTarget::GetAnchoredMessageIcon() const {
  return {};
}

ui::ImageModel TestCueTarget::GetOmniboxChipIcon() const {
  return {};
}

CueActionData TestCueTarget::CueActionDataFromResponse(
    const optimization_guide::proto::ContextualCue& cue,
    std::vector<tabs::TabHandle> tabs_to_show) const {
  GlicCueActionData data;
  if (cue.has_gemini_in_chrome_surface()) {
    data.prompt = cue.gemini_in_chrome_surface().prompt();
  }
  data.tabs_to_share = std::move(tabs_to_show);
  return data;
}

optimization_guide::proto::ContextualCueingSurface TestCueTarget::GetSurface()
    const {
  return optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_GEMINI_IN_CHROME;
}

}  // namespace contextual_cueing
