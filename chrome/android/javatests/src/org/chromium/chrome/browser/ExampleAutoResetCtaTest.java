// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;

/**
 * Example [instrumentation/on-device] [integration/app-wide], [batched], [activity-reused] test
 * that resets tab state between test cases.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ExampleAutoResetCtaTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Test
    @LargeTest
    public void testOnBlankPage1() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        page.openNewTabFast();
    }

    @Test
    @LargeTest
    public void testOnBlankPage2() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        page.openNewIncognitoTabOrWindowFast();
    }
}
