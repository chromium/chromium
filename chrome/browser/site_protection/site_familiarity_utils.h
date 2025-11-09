// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_

#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"

class Profile;

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

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_UTILS_H_
