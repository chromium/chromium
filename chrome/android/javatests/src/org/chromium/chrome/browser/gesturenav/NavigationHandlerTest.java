// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.app.Activity;
import android.graphics.Point;
import android.support.test.InstrumentationRegistry;
import android.util.DisplayMetrics;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
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
    private static final int PAGELOAD_TIMEOUT_MS = 4000;

    private EmbeddedTestServer mTestServer;
    private HistoryNavigationLayout mNavigationLayout;
    private NavigationHandler mNavigationHandler;
    private float mEdgeWidthPx;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        CompositorAnimationHandler.setTestingMode(true);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getMetrics(
                displayMetrics);
        mEdgeWidthPx = displayMetrics.density * NavigationHandler.EDGE_WIDTH_DP;
        HistoryNavigationCoordinator coordinator = getNavigationCoordinator();
        mNavigationLayout = coordinator.getLayoutForTesting();
        mNavigationHandler = coordinator.getNavigationHandlerForTesting();
    }

    @After
    public void tearDown() {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
    }

    private HistoryNavigationCoordinator getNavigationCoordinator() {
        TabbedRootUiCoordinator uiCoordinator =
                (TabbedRootUiCoordinator) mActivityTestRule.getActivity()
                        .getRootUiCoordinatorForTesting();
        return uiCoordinator.getHistoryNavigationCoordinatorForTesting();
    }

    private Tab currentTab() {
        return mActivityTestRule.getActivity().getActivityTabProvider().get();
    }

    private void loadNewTabPage() {
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), false, true);
    }

    private void assertNavigateOnSwipeFrom(boolean edge, String toUrl) {
        ChromeTabUtils.waitForTabPageLoaded(currentTab(), toUrl, () -> swipeFromEdge(edge), 10);
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(currentTab()),
                                Matchers.is(toUrl)));
        Assert.assertEquals(
                "Didn't navigate back", toUrl, ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    private void swipeFromEdge(boolean leftEdge) {
        Point size = new Point();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        final float startx = leftEdge ? mEdgeWidthPx / 2 : size.x - mEdgeWidthPx / 2;
        final float endx = size.x / 2;
        final float yMiddle = size.y / 2;
        swipe(leftEdge, startx, endx, yMiddle);
    }

    // Make an edge swipe too short to trigger the navigation.
    private void shortSwipeFromEdge(boolean leftEdge) {
        Point size = new Point();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        final float startx = leftEdge ? 0 : size.x;
        final float endx = leftEdge ? mEdgeWidthPx : size.x - mEdgeWidthPx;
        final float yMiddle = size.y / 2;
        swipe(leftEdge, startx, endx, yMiddle);
    }

    private void swipe(boolean leftEdge, float startx, float endx, float y) {
        // # of pixels (of reasonally small value) which a finger moves across
        // per one motion event.
        final float distancePx = 6.0f;
        final float step = Math.signum(endx - startx) * distancePx;
        final int eventCounts = (int) ((endx - startx) / step);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mNavigationHandler.onDown();
            float nextx = startx + step;
            for (int i = 0; i < eventCounts; i++, nextx += step) {
                mNavigationHandler.onScroll(startx, -step, 0, nextx, y);
            }
            mNavigationHandler.release(true);
        });
    }

    @Test
    @SmallTest
    public void testShortSwipeDoesNotTriggerNavigation() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        shortSwipeFromEdge(LEFT_EDGE);
        CriteriaHelper.pollUiThread(mNavigationLayout::isLayoutDetached,
                "Navigation Layout should be detached after use");
        Assert.assertEquals("Current page should not change", UrlConstants.NTP_URL,
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    @Test
    @SmallTest
    public void testCloseChromeAtHistoryStackHead() {
        loadNewTabPage();
        final Activity activity = mActivityTestRule.getActivity();
        swipeFromEdge(LEFT_EDGE);
        CriteriaHelper.pollUiThread(() -> {
            int state = ApplicationStatus.getStateForActivity(activity);
            return state == ActivityState.STOPPED || state == ActivityState.DESTROYED;
        }, "Chrome should be in background");
    }

    @Test
    @SmallTest
    public void testLayoutGetsDetachedAfterUse() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        swipeFromEdge(LEFT_EDGE);
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
        Tab oldTab = currentTab();
        TabCreator tabCreator = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getTabCreator(false));
        Tab newTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return tabCreator.createNewTab(
                    new LoadUrlParams(UrlConstants.RECENT_TABS_URL, PageTransition.LINK),
                    TabLaunchType.FROM_LINK, oldTab);
        });
        Assert.assertEquals(newTab, currentTab());
        swipeFromEdge(LEFT_EDGE);

        // Assert that the new tab was closed and the old tab is the current tab again.
        CriteriaHelper.pollUiThread(() -> !newTab.isInitialized());
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
        Assert.assertFalse(mNavigationHandler.triggerUi(LEFT_EDGE, 0, 0));

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
        Assert.assertFalse(mNavigationHandler.triggerUi(/*forward=*/false, 0, 0));
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testEdgeSwipeIsNoopInTabSwitcher() throws TimeoutException {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        setTabSwitcherModeAndWait(true);
        swipeFromEdge(LEFT_EDGE);
        Assert.assertTrue("Chrome should stay in tab switcher",
                mActivityTestRule.getActivity().isInOverviewMode());
        setTabSwitcherModeAndWait(false);
        Assert.assertEquals("Current page should not change", UrlConstants.RECENT_TABS_URL,
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    /**
     * Enter or exit the tab switcher with animations and wait for the scene to change.
     * @param inSwitcher Whether to enter or exit the tab switcher.
     */
    private void setTabSwitcherModeAndWait(boolean inSwitcher) throws TimeoutException {
        CallbackHelper switchHelper = new CallbackHelper();
        LayoutStateProvider.LayoutStateObserver layoutObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if ((inSwitcher && layoutType == LayoutType.TAB_SWITCHER)
                                || (!inSwitcher && layoutType == LayoutType.BROWSING)) {
                            switchHelper.notifyCalled();
                        }
                    }
                };

        LayoutManagerChrome controller = mActivityTestRule.getActivity().getLayoutManager();
        controller.addObserver(layoutObserver);
        if (inSwitcher) {
            TestThreadUtils.runOnUiThreadBlocking(() -> controller.showOverview(false));
        } else {
            TestThreadUtils.runOnUiThreadBlocking(() -> controller.hideOverview(false));
        }
        switchHelper.waitForCallback(0);
        controller.removeObserver(layoutObserver);
    }
}
