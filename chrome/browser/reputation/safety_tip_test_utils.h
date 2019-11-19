// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_SAFETY_TIP_TEST_UTILS_H_
#define CHROME_BROWSER_REPUTATION_SAFETY_TIP_TEST_UTILS_H_

#include <string>
#include <vector>

#include "chrome/browser/reputation/safety_tips.pb.h"

// Initialize component configuration. Necessary to enable Safety Tips for
// testing, as no heuristics trigger if the allowlist is inaccessible.
void InitializeSafetyTipConfig();

// Sets the patterns included in component with the given flag type for tests.
void SetSafetyTipPatternsWithFlagType(
    std::vector<std::string> pattern,
    chrome_browser_safety_tips::FlaggedPage::FlagType type);

// Sets the patterns to trigger a bad-reputation Safety Tip for tests. This just
// calls SetSafetyTipPatternsWithFlagType with BAD_REPUTATION as the type.
void SetSafetyTipBadRepPatterns(std::vector<std::string> pattern);

// Sets allowlist patterns in the given proto for testing.
void SetSafetyTipAllowlistPatterns(std::vector<std::string> patterns);

#endif  // CHROME_BROWSER_REPUTATION_SAFETY_TIP_TEST_UTILS_H_
