// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocked_action_waiter.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"

namespace extensions {

BlockedActionWaiter::BlockedActionWaiter(ExtensionActionRunner* runner) {
  action_runner_observation_.Observe(runner);
}

BlockedActionWaiter::~BlockedActionWaiter() = default;

void BlockedActionWaiter::Wait() {
  run_loop_.Run();
}

void BlockedActionWaiter::OnBlockedActionAdded() {
  run_loop_.Quit();
}

}  // namespace extensions
