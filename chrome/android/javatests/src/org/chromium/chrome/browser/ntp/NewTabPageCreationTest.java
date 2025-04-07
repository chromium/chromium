// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;

/** Tests for creating a tab with NewTabPage. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class NewTabPageCreationTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Test
    @MediumTest
    public void testCreateNTP() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("NewTabPage.OpenedInNewTab", 2 /* FROM_CHROME_UI */)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivityTestRule.getActivity().getCurrentTabCreator().launchNtp();
                });

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        Tab currentTab = sActivityTestRule.getActivity().getActivityTab();
        assertNotNull(currentTab);
        NativePage nativePage = currentTab.getNativePage();
        assertNotNull(nativePage);
        assertTrue(nativePage instanceof NewTabPage);
    }
}
