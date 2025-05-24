// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTION_WAITER_H_
#define CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTION_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Can be used to wait for blocked actions (pending scripts, web requests, etc.)
// to be noticed in tests. Blocked actions recording initiates in the renderer
// so this helps when waiting from the browser side. This should be used on the
// stack for proper destruction.
class BlockedActionWaiter : public ExtensionActionRunner::TestObserver {
 public:
  // `runner` must outlive this object.
  explicit BlockedActionWaiter(ExtensionActionRunner* runner);
  BlockedActionWaiter(const BlockedActionWaiter&) = delete;
  BlockedActionWaiter& operator=(const BlockedActionWaiter&) = delete;
  ~BlockedActionWaiter() override;

  // Wait for the blocked action until the observer is called with the blocked
  // action being added.
  void Wait();

 private:
  // ExtensionActionRunner::TestObserver:
  void OnBlockedActionAdded() override;

  base::ScopedObservation<ExtensionActionRunner,
                          ExtensionActionRunner::TestObserver>
      action_runner_observation_{this};
  base::RunLoop run_loop_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTION_WAITER_H_
