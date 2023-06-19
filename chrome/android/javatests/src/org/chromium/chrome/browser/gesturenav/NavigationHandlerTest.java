// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.os.SystemClock;
import android.view.MotionEvent;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.FeatureList;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Tests {@link NavigationHandler} navigating back/forward using overscroll history navigation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigationHandlerTest {
    private static final String RENDERED_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final boolean LEFT_EDGE = true;
    private static final boolean RIGHT_EDGE = false;

    private EmbeddedTestServer mTestServer;
    private HistoryNavigationLayout mNavigationLayout;
    private NavigationHandler mNavigationHandler;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private GestureNavigationUtils mNavUtils;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        CompositorAnimationHandler.setTestingMode(true);
        mNavUtils = new GestureNavigationUtils(mActivityTestRule);
        mNavigationHandler = mNavUtils.getNavigationHandler();
        mNavigationLayout = mNavUtils.getLayout();
    }

    @After
    public void tearDown() {
        CompositorAnimationHandler.setTestingMode(false);
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
    }

    private Tab currentTab() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getActivityTabProvider().get());
    }

    private void loadNewTabPage() {
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), false, true);
    }

    private void assertNavigateOnSwipeFrom(boolean edge, String toUrl) {
        ChromeTabUtils.waitForTabPageLoaded(
                currentTab(), toUrl, () -> mNavUtils.swipeFromEdge(edge), 10);
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(currentTab()),
                                Matchers.is(toUrl)));
        Assert.assertEquals(
                "Didn't navigate back", toUrl, ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
        Assert.assertEquals("Detected a wrong direction.", mNavigationHandler.fromLeftSide(), edge);
    }

    @Test
    @SmallTest
    public void testShortSwipeDoesNotTriggerNavigation() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mNavUtils.shortSwipeFromEdge(LEFT_EDGE);
        CriteriaHelper.pollUiThread(mNavigationLayout::isLayoutDetached,
                "Navigation Layout should be detached after use");
        Assert.assertEquals("Current page should not change", UrlConstants.NTP_URL,
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
        Assert.assertTrue(
                "The gesture should start from the left side.", mNavigationHandler.fromLeftSide());
    }

    @Test
    @SmallTest
    public void testCloseChromeAtHistoryStackHead() {
        loadNewTabPage();
        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        mNavUtils.swipeFromLeftEdge();
        CriteriaHelper.pollUiThread(
                AsyncInitializationActivity::wasMoveTaskToBackInterceptedForTesting);
    }

    @Test
    @SmallTest
    public void testLayoutGetsDetachedAfterUse() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        mNavUtils.swipeFromLeftEdge();
        CriteriaHelper.pollUiThread(mNavigationLayout::isLayoutDetached,
                "Navigation Layout should be detached after use");
        Assert.assertNull(mNavigationLayout.getDetachLayoutRunnable());
    }

    @Test
    @SmallTest
    public void testReleaseGlowWithoutPrecedingPullIgnored() {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Right swipe on a rendered page to initiate overscroll glow.
            mNavigationHandler.onDown();
            mNavigationHandler.triggerUi(true, 0, 0);

            // Test that a release without preceding pull requests works
            // without crashes.
            mNavigationHandler.release(true);
        });

        // Just check we're still on the same URL.
        Assert.assertEquals(mTestServer.getURL(RENDERED_PAGE),
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    @Test
    @SmallTest
    public void testSwipeNavigateOnNativePage() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        assertNavigateOnSwipeFrom(LEFT_EDGE, UrlConstants.NTP_URL);
        assertNavigateOnSwipeFrom(RIGHT_EDGE, UrlConstants.RECENT_TABS_URL);
    }

    @Test
    @SmallTest
    public void testSwipeNavigateOnRenderedPage() {
        // TODO(crbug.com/1426201): Write a test variation running with
        //     ChromeFeatureList.BACK_FORWARD_TRANSITIONS enabled when the feature is completed.
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        assertNavigateOnSwipeFrom(LEFT_EDGE, mTestServer.getURL(RENDERED_PAGE));
        assertNavigateOnSwipeFrom(RIGHT_EDGE, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    public void testLeftEdgeSwipeClosesTabLaunchedFromLink() {
        FeatureList.setTestFeatures(Map.of(ChromeFeatureList.BACK_FORWARD_TRANSITIONS, false));
        testLeftEdgeSwipeClosesTabLaunchedFromLinkInternal();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1426201")
    public void testLeftEdgeSwipeClosesTabLaunchedFromLink_withBackForwardTransition() {
        FeatureList.setTestFeatures(Map.of(ChromeFeatureList.BACK_FORWARD_TRANSITIONS, true));
        testLeftEdgeSwipeClosesTabLaunchedFromLinkInternal();
    }

    private void testLeftEdgeSwipeClosesTabLaunchedFromLinkInternal() {
        Tab oldTab = currentTab();
        TabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getTabCreator(false));
        Tab newTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return tabCreator.createNewTab(
                    new LoadUrlParams(UrlConstants.RECENT_TABS_URL, PageTransition.LINK),
                    TabLaunchType.FROM_LINK, oldTab);
        });
        Assert.assertEquals(newTab, currentTab());
        mNavUtils.swipeFromLeftEdge();

        // Assert that the new tab was closed and the old tab is the current tab again.
        CriteriaHelper.pollUiThread(() -> !newTab.isInitialized());
        Assert.assertNull("Not supposed to trigger an animation when closing tab",
                mNavigationHandler.getTabOnBackGestureHandlerForTesting());
        Assert.assertEquals(oldTab, currentTab());
        Assert.assertEquals("Chrome should remain in foreground", ActivityState.RESUMED,
                ApplicationStatus.getStateForActivity(mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    public void testSwipeAfterDestroy() {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        mNavigationHandler.destroy();

        // |triggerUi| can be invoked by SwipeRefreshHandler on the rendered
        // page. Make sure this won't crash after the handler(and also
        // handler action delegate) is destroyed.
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mNavigationHandler.triggerUi(LEFT_EDGE, 0, 0)));

        // Just check we're still on the same URL.
        Assert.assertEquals(mTestServer.getURL(RENDERED_PAGE),
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    @Test
    @SmallTest
    public void testSwipeAfterTabDestroy() {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        TestThreadUtils.runOnUiThreadBlocking(currentTab()::destroy);

        // |triggerUi| can be invoked by SwipeRefreshHandler on the rendered
        // page. Make sure this won't crash after the current tab is destroyed.
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mNavigationHandler.triggerUi(/*forward=*/false, 0, 0)));
    }

    @Test
    @SmallTest
    public void testSwipeAfterDestroyActivity_NativePage() {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        TestThreadUtils.runOnUiThreadBlocking(mActivityTestRule.getActivity()::finish);

        // CompositorViewHolder dispatches motion events and invoke the handler's
        // |handleTouchEvent| on native pages. Make sure this won't crash the app after
        // the handler is destroyed.
        long eventTime = SystemClock.uptimeMillis();
        MotionEvent e = MotionEvent.obtain(
                eventTime, eventTime, MotionEvent.ACTION_DOWN, /*x=*/10, /*y=*/100, 0);
        TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getCompositorViewHolderForTesting()
                                   .dispatchTouchEvent(e));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testEdgeSwipeIsNoopInTabSwitcher() throws TimeoutException {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        setTabSwitcherModeAndWait(true);
        mNavUtils.swipeFromLeftEdge();
        Assert.assertTrue("Chrome should stay in tab switcher",
                mActivityTestRule.getActivity().isInOverviewMode());
        setTabSwitcherModeAndWait(false);
        Assert.assertEquals("Current page should not change. ", UrlConstants.RECENT_TABS_URL,
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisabledTest(message = "https://crbug.com/1435090")
    public void testSwipeAndHoldOnNtp_EnterTabSwitcher() throws TimeoutException {
        // Clicking tab switcher button while swiping and holding the gesture navigation
        // bubble should reset the state and dismiss the UI.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mNavUtils.swipeFromEdgeAndHold(/*leftEdge=*/true);
        setTabSwitcherModeAndWait(true);
        Assert.assertFalse("Navigation UI should be reset.", mNavigationHandler.isActive());
    }

    /**
     * Enter or exit the tab switcher with animations and wait for the scene to change.
     * @param inSwitcher Whether to enter or exit the tab switcher.
     */
    private void setTabSwitcherModeAndWait(boolean inSwitcher) {
        LayoutManager layoutManager = mActivityTestRule.getActivity().getLayoutManager();
        @LayoutType
        int layout = inSwitcher ? LayoutType.TAB_SWITCHER : LayoutType.BROWSING;
        LayoutTestUtils.startShowingAndWaitForLayout(layoutManager, layout, false);
    }
}
