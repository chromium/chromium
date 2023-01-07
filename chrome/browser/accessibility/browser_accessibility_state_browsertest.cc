// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/browser_test.h"

namespace content {

// Note: even though BrowserAccessibilityStateImpl is in content, this
// test should be in Chrome because otherwise the Chrome-OS-specific
// histograms won't get updated.
class BrowserAccessibilityStateImplTest : public InProcessBrowserTest {
};

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityStateImplTest, TestHistograms) {
  base::HistogramTester histograms;

  BrowserAccessibilityState::GetInstance()->UpdateHistogramsForTesting();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  histograms.ExpectTotalCount("Accessibility.CrosSpokenFeedback", 1);
#endif
}

}  // namespace content
