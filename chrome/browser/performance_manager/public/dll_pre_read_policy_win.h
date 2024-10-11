// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_DLL_PRE_READ_POLICY_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_DLL_PRE_READ_POLICY_WIN_H_

#include <optional>

#include "base/feature_list.h"
#include "base/files/drive_info.h"
#include "base/time/time.h"

namespace performance_manager {

// Posts a task to a background thread to asynchronously determine whether the
// Chrome DLL resides on an SSD vs. a rotational drive, and whether the drive is
// removable. Must be invoked on startup, ideally as early as possible after the
// creation of threads, exactly once, before the first call to
// ShouldPreReadDllInChild(). If the task does not run before
// ShouldPreReaddllInchild(), the DLL will not be prefetched (since the browser
// just did) -- if it's never called, then the DLL will wrongly never be
// prefetched.
void InitializeDllPrereadPolicy();

// Returns true if a child process should pre-read its main DLL.
bool ShouldPreReadDllInChild();

// Whether enough time has passed since the browser process was launched,
// meaning that users in `kNoPreReadMainDllStartup should prefetch once more.
bool StartupPrefetchTimeoutElapsed(base::TimeTicks now);

// Determines if `drive_info` indicates a disk that has no seek penalty, is
// not removable, and is not connected via a USB bus. In other words, has the
// highest liklihood of exceedingly fast performance.
bool IsFixedSsd(const std::optional<base::DriveInfo>& info);

// Set whether the chrome DLL is on a fixed SSD for testing purposes.
void SetChromeDllOnSsdForTesting(bool on_fixed_ssd);

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_DLL_PRE_READ_POLICY_WIN_H_
