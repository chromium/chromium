// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TABBED_STARTUP_WINDOW_POLICY_DELEGATE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_TABBED_STARTUP_WINDOW_POLICY_DELEGATE_BRIDGE_H_

#include <string>
#include <vector>

class PrefService;

namespace chrome::android {

// Returns the validated web-safe startup URLs stored in `pref_service`.
std::vector<std::string> GetSessionStartupUrls(PrefService* pref_service);

// Persists `urls` as web-safe startup URLs in `pref_service`, resetting the
// startup type to `SessionStartupPref::DEFAULT` if no valid URLs remain.
void SetSessionStartupUrls(PrefService* pref_service,
                           const std::vector<std::string>& urls);

}  // namespace chrome::android

#endif  // CHROME_BROWSER_ANDROID_TABBED_STARTUP_WINDOW_POLICY_DELEGATE_BRIDGE_H_
