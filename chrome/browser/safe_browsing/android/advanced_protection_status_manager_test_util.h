// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_ADVANCED_PROTECTION_STATUS_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_ADVANCED_PROTECTION_STATUS_MANAGER_TEST_UTIL_H_

namespace safe_browsing {

// Sets the OS-requested advanced protection state.
void SetAdvancedProtectionStateForTesting(
    bool is_advanced_protection_requested_by_os);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_ADVANCED_PROTECTION_STATUS_MANAGER_TEST_UTIL_H_
