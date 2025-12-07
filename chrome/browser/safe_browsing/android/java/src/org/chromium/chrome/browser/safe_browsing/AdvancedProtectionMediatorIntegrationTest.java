// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;

/** Integration test for {@link AdvancedProtectionMediator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AdvancedProtectionMediatorIntegrationTest {
    @ClassRule
    public static final AdvancedProtectionTestRule sAdvancedProtectionTestRule =
            new AdvancedProtectionTestRule();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String ADVANCED_PROTECTION_UMA =
            "SafeBrowsing.Android.AdvancedProtection.Enabled";

    @Before
    public void setUp() {
        sAdvancedProtectionTestRule.setIsAdvancedProtectionRequestedByOs(false);
    }

    @Test
    @MediumTest
    public void testUmaOnStartup_AdvancedProtectionEnabled() {
        sAdvancedProtectionTestRule.setIsAdvancedProtectionRequestedByOs(true);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(ADVANCED_PROTECTION_UMA, true);
        mActivityTestRule.startOnBlankPage();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    public void testUmaOnStartup_AdvancedProtectionDisabled() {
        sAdvancedProtectionTestRule.setIsAdvancedProtectionRequestedByOs(false);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(ADVANCED_PROTECTION_UMA, false);
        mActivityTestRule.startOnBlankPage();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }
}
