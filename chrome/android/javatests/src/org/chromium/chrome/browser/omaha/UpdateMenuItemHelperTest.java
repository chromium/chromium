// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.browser.ui.appmenu.TestAppMenuObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.TimeoutException;

/** Tests for the UpdateMenuItemHelper. */
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
        public String getCurrentlyUsedVersion() {
            Assert.assertNotNull("Never set the current version", mCurrentVersion);
            mAskedForCurrentVersion = true;
            return mCurrentVersion;
        }

        @Override
        public String getLatestKnownVersion() {
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

    /** Reports a test market URL back to OmahaClient. */
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
        VersionNumberGetter.setEnableUpdateDetectionForTesting(true);
    }

    /**
     * Prepares Main before actually launching it. This is required since we don't have all of the
     * info we need in setUp().
     *
     * @param currentVersion Version to report as the current version of Chrome
     * @param latestVersion Version to report is available by Omaha
     */
    private void prepareAndStartMainActivity(String currentVersion, String latestVersion) {
        // Report fake versions back to Main when it asks.
        mMockVersionNumberGetter = new MockVersionNumberGetter(currentVersion, latestVersion);
        VersionNumberGetter.setInstanceForTests(mMockVersionNumberGetter);

        // Report a test URL to Omaha.
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
                () -> {
                    Criteria.checkThat(
                            mMockVersionNumberGetter.askedForCurrentVersion(), Matchers.is(true));
                    Criteria.checkThat(
                            mMockVersionNumberGetter.askedForLatestVersion(), Matchers.is(true));
                },
                MS_TIMEOUT,
                MS_INTERVAL);
    }

    /** Checks that the menu item is shown when a new version is available. */
    private void checkUpdateMenuItemIsShowing(String currentVersion, String latestVersion)
            throws Exception {
        prepareAndStartMainActivity(currentVersion, latestVersion);
        showAppMenuAndAssertMenuShown();
        Assert.assertNotNull(
                "Update menu item is not showing.",
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.update_menu_id));
    }

    /** Checks that the menu item is not shown when a new version is not available. */
    private void checkUpdateMenuItemIsNotShowing(String currentVersion, String latestVersion)
            throws Exception {
        prepareAndStartMainActivity(currentVersion, latestVersion);
        showAppMenuAndAssertMenuShown();
        Assert.assertNull(
                "Update menu item is showing.",
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.update_menu_id));
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testCurrentVersionIsOlder() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    public void testCurrentVersionIsOlderAutomotive() throws Exception {
        checkUpdateMenuItemIsNotShowing("0.0.0.0", "1.2.3.4");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testCurrentVersionIsSame() throws Exception {
        checkUpdateMenuItemIsNotShowing("1.2.3.4", "1.2.3.4");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testCurrentVersionIsNewer() throws Exception {
        checkUpdateMenuItemIsNotShowing("27.0.1453.42", "26.0.1410.49");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testNoVersionKnown() throws Exception {
        checkUpdateMenuItemIsNotShowing("1.2.3.4", "0");
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testMenuItemNotShownInOverview() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");

        // checkUpdateMenuItemIsShowing() opens the menu; hide it and assert it's dismissed.
        hideAppMenuAndAssertMenuShown();

        // Enter the tab switcher.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, false);

        // Make sure the item is not shown in tab switcher app menu.
        showAppMenuAndAssertMenuShown();
        Assert.assertNull(
                "Update menu item is showing.",
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mActivityTestRule.getAppMenuCoordinator(), R.id.update_menu_id));
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testClickUpdateMenuItem() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");

        Intents.init();
        ActivityResult intentResult = new ActivityResult(Activity.RESULT_OK, null);
        Intents.intending(IntentMatchers.hasData(TEST_MARKET_URL)).respondWith(intentResult);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AppMenuTestSupport.callOnItemClick(
                                mActivityTestRule.getAppMenuCoordinator(), R.id.update_menu_id));

        Intents.intended(Matchers.allOf(IntentMatchers.hasData(TEST_MARKET_URL)));

        mMenuObserver.menuHiddenCallback.waitForCallback(0);
        waitForAppMenuDimissedRunnable();

        Intents.release();
    }

    @Test
    @MediumTest
    @Feature({"Omaha"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testHideMenuWithoutClicking() throws Exception {
        checkUpdateMenuItemIsShowing("0.0.0.0", "1.2.3.4");

        hideAppMenuAndAssertMenuShown();
        waitForAppMenuDimissedRunnable();
    }

    private void showAppMenuAndAssertMenuShown() throws TimeoutException {
        int currentCallCount = mMenuObserver.menuShownCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });
        mMenuObserver.menuShownCallback.waitForCallback(currentCallCount);
    }

    private void hideAppMenuAndAssertMenuShown() throws TimeoutException {
        int currentCallCount = mMenuObserver.menuHiddenCallback.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getAppMenuCoordinator().getAppMenuHandler().hideAppMenu());

        mMenuObserver.menuHiddenCallback.waitForCallback(currentCallCount);
    }

    private void waitForAppMenuDimissedRunnable() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return UpdateMenuItemHelper.getInstance(mActivityTestRule.getProfile(false))
                            .getMenuDismissedRunnableExecutedForTests();
                });
    }
}
