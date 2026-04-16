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
  CueActionData click_data = std::monostate();

  TestCueTarget();
  ~TestCueTarget() override;

  bool HasClickData() const;

  // CueTarget:
  bool IsEligible() const override;
  void OnClick(CueActionData data) override;
  ui::ImageModel GetAnchoredMessageIcon() const override;
  ui::ImageModel GetOmniboxChipIcon() const override;
  CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCueingResponse& response)
      const override;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_TEST_CUE_TARGET_H_
