// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.Bundle;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests the integration of {@link MenuDirectActionHandler} with {@link ChromeActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MenuDirectActionHandlerTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private MenuDirectActionHandler mHandler;
    private ChromeActivity mActivity;
    private TabModelSelector mTabModelSelector;

    @Before
    public void setUp() throws Exception {
        mActivity = sActivityTestRule.getActivity();
        mTabModelSelector = mActivity.getTabModelSelector();
        mHandler = new MenuDirectActionHandler(mActivity, mTabModelSelector);
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testPerformDirectActionThroughActivity() {
        mHandler.allowAllActions();

        List<Bundle> results = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(
                    mHandler.performDirectAction("new_tab", Bundle.EMPTY, (r) -> results.add(r)));
        });
        assertThat(results, Matchers.hasSize(1));
        assertEquals(2, mTabModelSelector.getTotalTabCount());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertFalse(mHandler.performDirectAction(
                    "doesnotexist", Bundle.EMPTY, (r) -> fail("Unexpected result: " + r)));
        });
    }

    @Test
    @SmallTest
    @Feature({"DirectActions"})
    public void testReportAvailableActions() {
        mHandler.allowAllActions();

        assertThat(getDirectActions(),
                Matchers.containsInAnyOrder("bookmark_this_page", "reload", "downloads", "help",
                        "new_tab", "open_history", "preferences", "close_all_tabs"));

        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabModelSelector.closeAllTabs(); });
        // Wait for any pending animations for tab closures to complete.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mTabModelSelector.getTotalTabCount(), Matchers.is(0)));
        assertThat(getDirectActions(),
                Matchers.containsInAnyOrder("downloads", "help", "new_tab", "preferences"));
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testRestrictAvailableActions() {
        // By default, MenuDirectActionHandler supports no actions.
        assertThat(getDirectActions(), Matchers.empty());

        // Allow new_tab, then downloads
        mHandler.allowlistActions(R.id.new_tab_menu_id);

        assertThat(getDirectActions(), Matchers.contains("new_tab"));

        mHandler.allowlistActions(R.id.downloads_menu_id);
        assertThat(getDirectActions(), Matchers.containsInAnyOrder("new_tab", "downloads"));

        // Other actions cannot be called.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertFalse(mHandler.performDirectAction(
                    "help", Bundle.EMPTY, (r) -> fail("Unexpected result: " + r)));
        });

        // Allow all actions. This allows "help", never allowlisted explicitly
        mHandler.allowAllActions();
        assertThat(getDirectActions(), Matchers.hasItem("help"));

        // Allowlisting extra actions is ignored and does not hide the other actions.
        mHandler.allowlistActions(R.id.new_tab_menu_id);

        assertThat(getDirectActions(), Matchers.hasItem("help"));
    }

    private List<String> getDirectActions() {
        FakeDirectActionReporter reporter = new FakeDirectActionReporter();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.reportAvailableDirectActions(reporter); });
        return reporter.getDirectActions();
    }
}
