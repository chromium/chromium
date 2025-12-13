// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_

#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace site_protection {

// Returns whether v8-optimizations are disabled by default on sites which are
// unfamiliar to the user. Site familiarity is computed using a heuristic based
// on the user's navigation history and the safe-browsing
// high-confidence-allowlist.
bool AreV8OptimizationsDisabledOnUnfamiliarSites(Profile* profile);

// Computes the default Javascript-Optimizer setting. Ignores content-setting
// exceptions.
content_settings::JavascriptOptimizerSetting
ComputeDefaultJavascriptOptimizerSetting(Profile* profile);

// Checks if V8 optimizations are disabled in the renderer process of the given
// WebContents. Returns nullopt if the web_contents or the associated renderer
// process are not available.
std::optional<bool> AreV8OptimizationsDisabled(
    content::WebContents* web_contents);

// Returns the source of the optimizer setting in the given WebContents, or
// nullopt if the content settings map or current URL is not available.
std::optional<content_settings::SettingSource>
GetJavascriptOptimizerSettingSource(content::WebContents* web_contents);

// Enables the v8 optimizations content setting for the current URL in the
// given WebContents. Does nothing if the content settings map or current URL
// is not available.
// Note: the updated setting won't take effect until a new browsing instance
// is started (e.g. a new tab is opened).
void EnableV8Optimizations(content::WebContents* web_contents);

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_
