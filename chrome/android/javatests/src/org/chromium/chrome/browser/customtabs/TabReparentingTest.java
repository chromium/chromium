// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.PackageManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Outcome;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.CctTransitTestRule;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CctPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.WebContentsElementProvider;
import org.chromium.content_public.browser.test.transit.WebPageFacility;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for the "open in Chrome" functionality that reparents a tab from a CustomTabActivity to a
 * ChromeTabbedActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabReparentingTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String SELECT_POPUP_PAGE = "/chrome/test/data/android/select.html";

    @Rule public CctTransitTestRule mCustomTabActivityTestRule = new CctTransitTestRule();

    private static class SelectPageFacility extends WebPageFacility {
        public final HtmlElement select = declareHtmlElement("#select");

        /** Clicks the select element and waits for its popup to show. */
        public void clickSelectToShowPopup() {
            WebContentsElementProvider host = (WebContentsElementProvider) mHostStation;
            select.clickTo()
                    .waitFor(
                            SimpleConditions.uiThreadCondition(
                                    "Select popup is visible",
                                    host.getWebContentsElement(),
                                    webContents ->
                                            Condition.whether(
                                                    WebContentsUtils.isSelectPopupVisible(
                                                            webContents))));
        }
    }

    private static class TestContext extends ContextWrapper {
        public TestContext(Context baseContext) {
            super(baseContext);
        }

        @Override
        public PackageManager getPackageManager() {
            return CustomTabsTestUtils.getDefaultBrowserOverridingPackageManager(
                    getPackageName(), super.getPackageManager());
        }
    }

    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        Context appContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        LibraryLoader.getInstance().ensureInitialized();
        TestContext testContext = new TestContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(testContext);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
    }

    /**
     * @see CustomTabsIntentTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                ApplicationProvider.getApplicationContext(), mTestPage);
    }

    private WebPageStation reparentAndVerifyTab(
            CctPageStation cctPage, BasePageStation.Builder<WebPageStation> builder) {
        final Tab tabToBeReparented = cctPage.getTab();
        final CallbackHelper tabHiddenHelper = new CallbackHelper();
        TabObserver observer =
                new TabObserver() {
                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        tabHiddenHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> tabToBeReparented.addObserver(observer));
        WebPageStation ctaPage =
                cctPage.runOnUiThreadTo(
                                () -> {
                                    CustomTabActivity cctActivity = cctPage.getActivity();
                                    cctActivity
                                            .getCustomTabActivityNavigationController()
                                            .openCurrentUrlInBrowser();
                                    assertNull(cctActivity.getActivityTab());
                                })
                        // openCurrentUrlInBrowser() moves the tab to a ChromeTabbedActivity in a
                        // new task and finishes the CustomTabActivity. inNewTask() doesn't exit
                        // |cctPage|; no later Trip in this test starts from it.
                        .reach(
                                Outcome.enter(
                                                builder.withTabAlreadySelected(tabToBeReparented)
                                                        .build())
                                        .inNewTask());
        ChromeActivity newActivity = ctaPage.getActivity();
        assertEquals(newActivity.getWindowAndroid(), tabToBeReparented.getWindowAndroid());
        assertEquals(
                newActivity.getWindowAndroid(),
                tabToBeReparented.getWebContents().getTopLevelNativeWindow());
        Assert.assertFalse(
                TabTestUtils.getDelegateFactory(tabToBeReparented)
                        instanceof CustomTabDelegateFactory);
        assertEquals(
                "The tab should never be hidden during the reparenting process",
                0,
                tabHiddenHelper.getCallCount());
        Assert.assertFalse(TabTestUtils.isCustomTab(tabToBeReparented));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabToBeReparented.removeObserver(observer);
                    for (TabObserver tabObserver :
                            TabTestUtils.getTabObservers(tabToBeReparented)) {
                        Assert.assertFalse(tabObserver instanceof CustomTabObserver);
                    }
                });
        return ctaPage;
    }

    /** Test whether a custom tab can be reparented to a new activity. */
    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/40904413")
    public void testTabReparentingBasic() {
        CctPageStation cctPage =
                mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                        createMinimalCustomTabIntent());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));
        reparentAndVerifyTab(cctPage, WebPageStation.newBuilder());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));
    }

    /**
     * Test whether a custom tab can be reparented to a new activity and the select element is still
     * interactable after reparenting.
     */
    @SmallTest
    @Test
    public void testTabReparentingSelectPopup() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        SelectPageFacility cctSelectPage = new SelectPageFacility();
        CctPageStation cctPage =
                mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                        CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                                ApplicationProvider.getApplicationContext(),
                                mTestServer.getURL(SELECT_POPUP_PAGE)),
                        CctPageStation.newBuilder().withFacility(cctSelectPage));

        cctSelectPage.clickSelectToShowPopup();

        SelectPageFacility ctaSelectPage = new SelectPageFacility();
        reparentAndVerifyTab(cctPage, WebPageStation.newBuilder().withFacility(ctaSelectPage));
        ctaSelectPage.clickSelectToShowPopup();
    }
}
