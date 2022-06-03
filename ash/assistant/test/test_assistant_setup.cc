// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/test_assistant_setup.h"

namespace ash {

TestAssistantSetup::TestAssistantSetup() = default;

TestAssistantSetup::~TestAssistantSetup() = default;

void TestAssistantSetup::StartAssistantOptInFlow(
    FlowType type,
    StartAssistantOptInFlowCallback callback) {
  if (callback_) {
    // If opt-in flow is already in progress, immediately return |false|. This
    // behavior is consistent w/ the actual browser-side implementation.
    std::move(callback).Run(false);
    return;
  }
  // Otherwise we cache the callback until FinishAssistantOptInFlow() is called.
  callback_ = std::move(callback);
}

bool TestAssistantSetup::BounceOptInWindowIfActive() {
  // If |callback_| exists, opt-in flow is in progress and the browser-side
  // implementation would return |true| after bouncing the dialog window.
  return !!callback_;
}

void TestAssistantSetup::FinishAssistantOptInFlow(bool completed) {
  DCHECK(callback_);
  std::move(callback_).Run(completed);
}

}  // namespace ash
