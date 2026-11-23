// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_GLIC_RELAUNCH_H_
#define CHROME_BROWSER_GLIC_COMMON_GLIC_RELAUNCH_H_

#include "base/auto_reset.h"
#include "base/functional/callback.h"

namespace glic {

// Restarts Chrome immediately, or runs the test callback set via
// `SetRestartCallbackForTesting()`.
void RestartBrowserNow();

// Runs `callback` instead of restarting Chrome in `RestartBrowserNow()` while
// the returned object is alive.
[[nodiscard]] base::AutoReset<base::RepeatingClosure>
SetRestartCallbackForTesting(base::RepeatingClosure callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_GLIC_RELAUNCH_H_
