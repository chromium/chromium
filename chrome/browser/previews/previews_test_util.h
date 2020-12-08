// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_TEST_UTIL_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_TEST_UTIL_H_

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

// Expected console output when defer preview is not applied to the test
// webpage.
static const char kNonDeferredPageExpectedOutput[] =
    "ScriptLog:_InlineScript_SyncScript_BodyEnd_DeveloperDeferScript_OnLoad";

// Expected console output when defer preview is applied to the test webpage.
static const char kDeferredPageExpectedOutput[] =
    "ScriptLog:_BodyEnd_InlineScript_SyncScript_DeveloperDeferScript_OnLoad";

// Runs sendLogToTest() JavaScript method in the current tab in |browser| and
// returns the result.
std::string GetScriptLog(Browser* browser);

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count);

// Previews InfoBar (which these tests trigger) does not work on Mac.
// See https://crbug.com/782322 for details. Also occasional flakes on win7
// (https://crbug.com/789542).
#if defined(OS_WIN) || defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_TEST_UTIL_H_
