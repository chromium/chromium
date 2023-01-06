// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_TEST_ASSISTANT_SETUP_H_
#define ASH_ASSISTANT_TEST_TEST_ASSISTANT_SETUP_H_

#include "ash/public/cpp/assistant/assistant_setup.h"
#include "base/functional/callback.h"

namespace ash {

// An implementation of AssistantSetup for use in unittests.
class TestAssistantSetup : public AssistantSetup {
 public:
  TestAssistantSetup();
  TestAssistantSetup(const TestAssistantSetup&) = delete;
  TestAssistantSetup& operator=(const TestAssistantSetup&) = delete;
  ~TestAssistantSetup() override;

  // AssistantSetup:
  void StartAssistantOptInFlow(
      FlowType type,
      StartAssistantOptInFlowCallback callback) override;
  bool BounceOptInWindowIfActive() override;

  // Invoke in unittests to finish opt-in flow with the desired state of
  // completion. Note that this API may only be called while opt-in flow is in
  // progress (as indicated by existence of |callback_|).
  void FinishAssistantOptInFlow(bool completed);

 private:
  // If |callback_| exists, that means that opt-in flow is in progress as far
  // as unittests are concerned.
  StartAssistantOptInFlowCallback callback_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_TEST_ASSISTANT_SETUP_H_
