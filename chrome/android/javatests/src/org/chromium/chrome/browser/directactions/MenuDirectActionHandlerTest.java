// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.Build;
import android.os.Bundle;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.SingleTabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests the integration of {@link MenuDirectActionHandler} with {@link ChromeActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.N)
public class MenuDirectActionHandlerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private MenuDirectActionHandler mHandler;
    private ChromeActivity mActivity;
    private TabModelSelector mTabModelSelector;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
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

        // Tabs can't be closed for SingleTab Activities.
        if (mTabModelSelector instanceof SingleTabModelSelector) return;
        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabModelSelector.closeAllTabs(); });
        // Wait for any pending animations for tab closures to complete.
        CriteriaHelper.pollUiThread(Criteria.equals(0, () -> mTabModelSelector.getTotalTabCount()));
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
        mHandler.whitelistActions(R.id.new_tab_menu_id);

        assertThat(getDirectActions(), Matchers.contains("new_tab"));

        mHandler.whitelistActions(R.id.downloads_menu_id);
        assertThat(getDirectActions(), Matchers.containsInAnyOrder("new_tab", "downloads"));

        // Other actions cannot be called.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertFalse(mHandler.performDirectAction(
                    "help", Bundle.EMPTY, (r) -> fail("Unexpected result: " + r)));
        });

        // Allow all actions. This allows "help", never whitelisted explicitly
        mHandler.allowAllActions();
        assertThat(getDirectActions(), Matchers.hasItem("help"));

        // Whitelisting extra actions is ignored and does not hide the other actions.
        mHandler.whitelistActions(R.id.new_tab_menu_id);

        assertThat(getDirectActions(), Matchers.hasItem("help"));
    }

    private List<String> getDirectActions() {
        FakeDirectActionReporter reporter = new FakeDirectActionReporter();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.reportAvailableDirectActions(reporter); });
        return reporter.getDirectActions();
    }
}
