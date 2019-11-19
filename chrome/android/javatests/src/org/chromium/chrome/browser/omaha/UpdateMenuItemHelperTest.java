// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.support.test.espresso.intent.Intents;
import android.support.test.espresso.intent.matcher.IntentMatchers;
import android.support.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.browser.ui.appmenu.TestAppMenuObserver;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the UpdateMenuItemHelper.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable_update_menu_item"})
public class UpdateMenuItemHelperTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_MARKET_URL =
            "https://play.google.com/store/apps/details?id=com.android.chrome";

    private static final long MS_TIMEOUT = 2000;
    private static final long MS_INTERVAL = 500;

    /** Reports versions that we want back to OmahaClient. */
    private static class MockVersionNumberGetter extends VersionNumberGetter {
        // Both of these strings must be of the format "#.#.#.#".
        private final String mCurrentVersion;
        private final String mLatestVersion;

        private boolean mAskedForCurrentVersion;
        private boolean mAskedForLatestVersion;

        public MockVersionNumberGetter(String currentVersion, String latestVersion) {
            mCurrentVersion = currentVersion;
            mLatestVersion = latestVersion;
        }

        @Override
        public String getCurrentlyUsedVersion(Context applicationContext) {
            Assert.assertNotNull("Never set the current version", mCurrentVersion);
            mAskedForCurrentVersion = true;
            return mCurrentVersion;
        }

        @Override
        public String getLatestKnownVersion(Context applicationContext) {
            Assert.assertNotNull("Never set the latest version", mLatestVersion);
            mAskedForLatestVersion = true;
            return mLatestVersion;
        }

        public boolean askedForCurrentVersion() {
            return mAskedForCurrentVersion;
        }

        public boolean askedForLatestVersion() {
            return mAskedForLatestVersion;
        }
    }

    /** Reports a dummy market URL back to OmahaClient. */
    private static class MockMarketURLGetter extends MarketURLGetter {
        private final String mURL;

        MockMarketURLGetter(String url) {
            mURL = url;
        }

        @Override
        protected String getMarketUrlInternal() {
            return mURL;
        }
    }


    private MockVersionNumberGetter mMockVersionNumberGetter;
    private MockMarketURLGetter mMockMarketURLGetter;
    private TestAppMenuObserver mMenuObserver;

    @Before
    public void setUp() {
        // This test explicitly tests for the menu item, so turn it on.
        VersionNumberGetter.setEnableUpdateDetection(true);
    }

    /**
     * Prepares Main before actually launching it.  This is required since we don't have all of the
     * info we need in setUp().
     * @param currentVersion Version to report as the current version of Chrome
     * @param latestVersion Version to report is available by Omaha
     */
    private void prepareAndStartMainActivity(String currentVersion, String latestVersion) {
        // Report fake versions back to Main when it asks.
        mMockVersionNumberGetter = new MockVersionNumberGetter(currentVersion, latestVersion);
        VersionNumberGetter.setInstanceForTests(mMockVersionNumberGetter);

        // Report a dummy URL to Omaha.
        mMockMarketURLGetter = new MockMarketURLGetter(TEST_MARKET_URL);
        MarketURLGetter.setInstanceForTests(mMockMarketURLGetter);

        // Start up main.
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mMenuObserver = new TestAppMenuObserver();
        mActivityTestRule.getAppMenuCoordinator().getAppMenuHandler().addObserver(mMenuObserver);

        // Check to make sure that the version numbers get queried.
        versionNumbersQueried();
    }

    private void versionNumbersQueried() {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mMockVersionNumberGetter.askedForCurrentVersion()
                                && mMockVersionNumberGetter.askedForLatestVersion();
                    }
                },
                MS_TIMEOUT, MS_INTERVAL);
    }

    /**
     * Checks that the menu item is shown when a new version is available.
     */
    private void checkUpdateMenuItemIsShowing(String currentVersion, String latestVersion)
            throws Exception {
        prepareAndStartMainActivity(currentVersion, latestVersion);
        showAppMenuAndAssertMenuShown();
        Assert.assertTrue("Update menu item is not showing.",
                mActivityTestRule.getMenu().findItem(R.id.update_menu_id).isVisible());
    }

    /**
     * Checks that the menu item is not shown when a new version is not available.
     */
    private void checkUpdateMenuItemIsNotShowing(String currentVersion, String latestVersion)
            throws Exception {
        prepareAndStartMainActivity(currentVersion, latestVersion);
        showAppMenuAndAssertMenuShown();
        Assert.assertFalse("Update menu item is showing.",
                mActivityTestRule.getMenu().findItem(R.id.update_menu_id).isVisible());
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @RetryOnFailure
    // TODO(https://crbug.com/965106): Fix tests when InlineUpdateFlow is enabled.
    @DisableFeatures("InlineUpdateFlow")
    public void testCurrentVersionIsOlder() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @RetryOnFailure
    public void testCurrentVersionIsSame() throws Exception {
        checkUpdateMenuItemIsNotShowing("1.2.3.4", "1.2.3.4");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    public void testCurrentVersionIsNewer() throws Exception {
        checkUpdateMenuItemIsNotShowing("27.0.1453.42", "26.0.1410.49");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @RetryOnFailure
    public void testNoVersionKnown() throws Exception {
        checkUpdateMenuItemIsNotShowing("1.2.3.4", "0");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
    // TODO(https://crbug.com/965106): Fix tests when InlineUpdateFlow is enabled.
    @DisableFeatures("InlineUpdateFlow")
    public void testMenuItemNotShownInOverview() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");

        // checkUpdateMenuItemIsShowing() opens the menu; hide it and assert it's dismissed.
        hideAppMenuAndAssertMenuShown();

        // Enter the tab switcher.
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getLayoutManager(), true, false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));
        overviewModeWatcher.waitForBehavior();

        // Make sure the item is not shown in tab switcher app menu.
        showAppMenuAndAssertMenuShown();
        Assert.assertFalse("Update menu item is showing.",
                mActivityTestRule.getMenu().findItem(R.id.update_menu_id).isVisible());
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @DisableFeatures("InlineUpdateFlow")
    public void testClickUpdateMenuItem() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");

        Assert.assertEquals(
                "Incorrect item clicked histogram count", 0, getTotalItemClickedCount());
        Assert.assertEquals(
                "Incorrect item not clicked histogram count", 0, getTotalItemNotClickedCount());

        Intents.init();
        ActivityResult intentResult = new ActivityResult(Activity.RESULT_OK, null);
        Intents.intending(IntentMatchers.hasData(TEST_MARKET_URL)).respondWith(intentResult);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AppMenuTestSupport.callOnItemClick(
                                mActivityTestRule.getAppMenuCoordinator(), R.id.update_menu_id));

        Intents.intended(Matchers.allOf(IntentMatchers.hasData(TEST_MARKET_URL)));

        Assert.assertEquals("Incorrect item clicked histogram count after item clicked", 1,
                getTotalItemClickedCount());
        Assert.assertEquals("Incorrect item not clicked histogram count after item clicked", 0,
                getTotalItemNotClickedCount());

        mMenuObserver.menuHiddenCallback.waitForCallback(0);
        waitForAppMenuDimissedRunnable();

        Assert.assertEquals("Incorrect item clicked histogram count after menu dismissed", 1,
                getTotalItemClickedCount());
        Assert.assertEquals("Incorrect item not clicked histogram count after menu dismissed", 0,
                getTotalItemNotClickedCount());

        Intents.release();
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @DisableFeatures("InlineUpdateFlow")
    public void testHideMenuWithoutClicking() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");

        Assert.assertEquals(
                "Incorrect item clicked histogram count", 0, getTotalItemClickedCount());
        Assert.assertEquals(
                "Incorrect item not clicked histogram count", 0, getTotalItemNotClickedCount());

        hideAppMenuAndAssertMenuShown();
        waitForAppMenuDimissedRunnable();

        Assert.assertEquals("Incorrect item clicked histogram count after menu dismissed", 0,
                getTotalItemClickedCount());
        Assert.assertEquals("Incorrect item not clicked histogram count after menu dismissed", 1,
                getTotalItemNotClickedCount());
    }

    private void showAppMenuAndAssertMenuShown() throws TimeoutException {
        int currentCallCount = mMenuObserver.menuShownCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });
        mMenuObserver.menuShownCallback.waitForCallback(currentCallCount);
    }

    private void hideAppMenuAndAssertMenuShown() throws TimeoutException {
        int currentCallCount = mMenuObserver.menuHiddenCallback.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getAppMenuCoordinator().getAppMenuHandler().hideAppMenu());

        mMenuObserver.menuHiddenCallback.waitForCallback(currentCallCount);
    }

    private int getTotalItemClickedCount() {
        return RecordHistogram.getHistogramValueCountForTesting(
                       UpdateMenuItemHelper.ACTION_TAKEN_ON_MENU_OPEN_HISTOGRAM,
                       UpdateMenuItemHelper.ITEM_CLICKED_INTENT_FAILED)
                + RecordHistogram.getHistogramValueCountForTesting(
                        UpdateMenuItemHelper.ACTION_TAKEN_ON_MENU_OPEN_HISTOGRAM,
                        UpdateMenuItemHelper.ITEM_CLICKED_INTENT_LAUNCHED);
    }

    private int getTotalItemNotClickedCount() {
        return RecordHistogram.getHistogramValueCountForTesting(
                UpdateMenuItemHelper.ACTION_TAKEN_ON_MENU_OPEN_HISTOGRAM,
                UpdateMenuItemHelper.ITEM_NOT_CLICKED);
    }

    private void waitForAppMenuDimissedRunnable() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return UpdateMenuItemHelper.getInstance()
                        .getMenuDismissedRunnableExecutedForTests();
            }
        });
    }
}

