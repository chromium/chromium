// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_

namespace performance_manager::user_tuning {

// Convenience shortcut for metrics code.
// Returns true if battery saver mode is available (via Finch) and battery saver
// mode is currently active.
bool IsRefreshRateThrottled();

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_TUNING_UTILS_H_
