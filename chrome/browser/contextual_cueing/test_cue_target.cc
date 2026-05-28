// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/test_cue_target.h"

#include "base/logging.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"

namespace contextual_cueing {

TestCueTarget::TestCueTarget() = default;
TestCueTarget::~TestCueTarget() = default;

bool TestCueTarget::HasClickData() const {
  return !std::holds_alternative<std::monostate>(click_data);
}

bool TestCueTarget::IsEligible() const {
  return eligible;
}

void TestCueTarget::OnClick(CueActionData data) {
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
