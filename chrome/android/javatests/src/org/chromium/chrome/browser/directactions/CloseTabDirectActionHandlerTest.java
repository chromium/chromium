// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/** Tests {@link CloseTabDirectActionHandler}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class CloseTabDirectActionHandlerTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private TabModelSelector mSelector;
    private CloseTabDirectActionHandler mHandler;

    @Before
    public void setUp() throws Exception {
        // Setup an activity with two blank tabs.
        sActivityTestRule.loadUrlInNewTab(
                "about:blank", false /* incognito */, TabLaunchType.FROM_CHROME_UI);

        mSelector = sActivityTestRule.getActivity().getTabModelSelector();
        mHandler = new CloseTabDirectActionHandler(mSelector);
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testCloseTabs() {
        // Actions are available
        assertThat(getDirectActions(), Matchers.containsInAnyOrder("close_tab"));

        // Close current tab
        Tab initiallyCurrent = mSelector.getCurrentTab();
        performAction("close_tab");
        assertThat(
                mSelector.getCurrentTab(), Matchers.not(Matchers.sameInstance(initiallyCurrent)));

        assertEquals(1, mSelector.getTotalTabCount());
        // Close last tab
        performAction("close_tab");

        // No tabs are left, so actions aren't available anymore.
        assertThat(getDirectActions(), Matchers.empty());
    }

    private void performAction(String name) {
        List<Bundle> responses = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(mHandler.performDirectAction(name, Bundle.EMPTY, (r) -> responses.add(r)));
        });
        assertThat(responses, Matchers.hasSize(1));
    }

    private List<String> getDirectActions() {
        FakeDirectActionReporter reporter = new FakeDirectActionReporter();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHandler.reportAvailableDirectActions(reporter));
        return reporter.getDirectActions();
    }
}
