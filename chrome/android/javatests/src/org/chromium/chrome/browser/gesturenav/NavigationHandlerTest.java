// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.os.Build;
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
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.native_page.BasicSmoothTransitionDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.UiAndroidFeatures;

import java.util.Map;
import java.util.concurrent.TimeoutException;

/** Tests {@link NavigationHandler} navigating back/forward using overscroll history navigation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Build(
        sdk_is_greater_than = Build.VERSION_CODES.Q,
        message = "crbug.com/1276402 crbug.com/345352689")
@Batch(Batch.PER_CLASS)
public class NavigationHandlerTest {
    private static final String RENDERED_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";
    private static final boolean LEFT_EDGE = true;
    private static final boolean RIGHT_EDGE = false;

    private EmbeddedTestServer mTestServer;
    private HistoryNavigationLayout mNavigationLayout;
    private NavigationHandler mNavigationHandler;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private GestureNavigationTestUtils mNavUtils;

    @Before
    public void setUp() throws InterruptedException {
        TestAnimations.setEnabled(true);
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));
        CompositorAnimationHandler.setTestingMode(true);
        mNavUtils = new GestureNavigationTestUtils(mActivityTestRule);
        mNavigationHandler = mNavUtils.getNavigationHandler();
        mNavigationLayout = mNavUtils.getLayout();
    }

    @After
    public void tearDown() {
        CompositorAnimationHandler.setTestingMode(false);
        setRtlForTesting(false);
    }

    private Tab currentTab() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getActivityTabProvider().get());
    }

    private void loadNewTabPage() {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                false,
                true);
    }

    private void assertNavigateOnSwipeFrom(boolean edge, String toUrl) {
        ChromeTabUtils.waitForTabPageLoaded(
                currentTab(), toUrl, () -> mNavUtils.swipeFromEdge(edge), 10);
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                ChromeTabUtils.getUrlStringOnUiThread(currentTab()),
                                Matchers.is(toUrl)));
        Assert.assertEquals(
                "Didn't navigate back", toUrl, ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
        Assert.assertEquals(
                "Detected a wrong direction.",
                mNavigationHandler.getInitiatingEdge() == BackGestureEventSwipeEdge.LEFT,
                edge);
    }

    /**
     * Enter or exit the tab switcher with animations and wait for the scene to change.
     *
     * @param inSwitcher Whether to enter or exit the tab switcher.
     */
    private void setTabSwitcherModeAndWait(boolean inSwitcher) {
        LayoutManager layoutManager = mActivityTestRule.getActivity().getLayoutManager();
        @LayoutType int layout = inSwitcher ? LayoutType.TAB_SWITCHER : LayoutType.BROWSING;
        LayoutTestUtils.startShowingAndWaitForLayout(layoutManager, layout, false);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=BackForwardTransitions<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:transition_from_native_pages/true/"
                + "transition_to_native_pages/true"
    })
    public void testSwipeBackToNTPWithTransition() throws InterruptedException {
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));

        mNavUtils.swipeFromEdgeAndHold(true);
        Assert.assertEquals(
                "Back forward transition not invoked yet",
                AnimationStage.OTHER,
                tab.getWebContents().getCurrentBackForwardTransitionStage());

        ThreadUtils.runOnUiThreadBlocking(() -> mNavigationHandler.release(true));
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        AnimationStage.INVOKE_ANIMATION
                                == tab.getWebContents().getCurrentBackForwardTransitionStage(),
                "invoking animation should be started");
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        AnimationStage.NONE
                                == tab.getWebContents().getCurrentBackForwardTransitionStage(),
                "should wait for animation to be finished");
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        ((NewTabPage) tab.getNativePage()).getSmoothTransitionDelegateForTesting()
                                != null,
                "Smooth transition should be enabled");
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        !((BasicSmoothTransitionDelegate)
                                        ((NewTabPage) tab.getNativePage())
                                                .getSmoothTransitionDelegateForTesting())
                                .getAnimatorForTesting()
                                .isRunning(),
                "Smooth transition should be finished");
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=BackForwardTransitions<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:transition_from_native_pages/true/"
                + "transition_to_native_pages/false"
    })
    public void testSwipeBackToNTPWithoutTransition() throws InterruptedException {
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));

        mNavUtils.swipeFromEdgeAndHold(true);
        Assert.assertEquals(
                "Back forward transition is not enabled for native pages",
                AnimationStage.NONE,
                tab.getWebContents().getCurrentBackForwardTransitionStage());

        ThreadUtils.runOnUiThreadBlocking(() -> mNavigationHandler.release(true));
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        AnimationStage.NONE
                                == tab.getWebContents().getCurrentBackForwardTransitionStage(),
                "Back forward transition is not enabled for native pages");
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=BackForwardTransitions<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:transition_from_native_pages/false/"
                + "transition_to_native_pages/false"
    })
    public void testSwipeBackFromNTPWithoutTransition() throws InterruptedException {
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        mNavUtils.swipeFromEdgeAndHold(true);
        Assert.assertEquals(
                "Back forward transition is not enabled for native pages",
                AnimationStage.NONE,
                tab.getWebContents().getCurrentBackForwardTransitionStage());

        ThreadUtils.runOnUiThreadBlocking(() -> mNavigationHandler.release(true));
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        AnimationStage.NONE
                                == tab.getWebContents().getCurrentBackForwardTransitionStage(),
                "Back forward transition is not enabled for native pages");
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=BackForwardTransitions<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:transition_from_native_pages/true/"
                + "transition_to_native_pages/true"
    })
    public void testSwipeBackToNativeBookmarksPageWithTransition() throws InterruptedException {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        mActivityTestRule.loadUrl("chrome-native://bookmarks/folder/0");
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));

        mNavUtils.swipeFromEdgeAndHold(true);
        Assert.assertEquals(
                "Back forward transition not invoked yet",
                AnimationStage.OTHER,
                tab.getWebContents().getCurrentBackForwardTransitionStage());

        ThreadUtils.runOnUiThreadBlocking(() -> mNavigationHandler.release(true));
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        AnimationStage.INVOKE_ANIMATION
                                == tab.getWebContents().getCurrentBackForwardTransitionStage(),
                "invoking animation should be started");
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        AnimationStage.NONE
                                == tab.getWebContents().getCurrentBackForwardTransitionStage(),
                "should wait for animation to be finished");
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        ((BookmarkPage) tab.getNativePage()).getSmoothTransitionDelegateForTesting()
                                != null,
                "Smooth transition should be enabled");
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        !((BasicSmoothTransitionDelegate)
                                        ((BookmarkPage) tab.getNativePage())
                                                .getSmoothTransitionDelegateForTesting())
                                .getAnimatorForTesting()
                                .isRunning(),
                "Smooth transition should be finished");
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=BackForwardTransitions<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:transition_from_native_pages/true/"
                + "transition_to_native_pages/true"
    })
    public void testSwipeBackWithoutTransition_AnimationsDisabled() throws InterruptedException {
        TestAnimations.setEnabled(false);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));

        mNavUtils.swipeFromEdgeAndHold(true);
        Assert.assertEquals(
                "Back forward transition is disabled due to no animation",
                AnimationStage.NONE,
                tab.getWebContents().getCurrentBackForwardTransitionStage());

        ThreadUtils.runOnUiThreadBlocking(() -> mNavigationHandler.release(true));
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        AnimationStage.NONE
                                == tab.getWebContents().getCurrentBackForwardTransitionStage(),
                "Back forward transition is disabled due to no animation");
    }

    @Test
    @SmallTest
    public void testShortSwipeDoesNotTriggerNavigation() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mNavUtils.shortSwipeFromEdge(LEFT_EDGE);
        CriteriaHelper.pollUiThread(
                mNavigationLayout::isLayoutDetached,
                "Navigation Layout should be detached after use");
        Assert.assertEquals(
                "Current page should not change",
                UrlConstants.NTP_URL,
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
        Assert.assertTrue(
                "The gesture should start from the left side.",
                mNavigationHandler.getInitiatingEdge() == BackGestureEventSwipeEdge.LEFT);
    }

    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338972492
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
        CriteriaHelper.pollUiThread(
                mNavigationLayout::isLayoutDetached,
                "Navigation Layout should be detached after use");
        Assert.assertNull(mNavigationLayout.getDetachLayoutRunnable());
    }

    @Test
    @SmallTest
    public void testReleaseGlowWithoutPrecedingPullIgnored() {
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Right swipe on a rendered page to initiate overscroll glow.
                    mNavigationHandler.onDown();
                    mNavigationHandler.triggerUi(
                            BackGestureEventSwipeEdge.RIGHT,
                            NavigationHandler.TriggerUiCallSource.ON_SCROLL);

                    // Test that a release without preceding pull requests works
                    // without crashes.
                    mNavigationHandler.release(true);
                });

        // Just check we're still on the same URL.
        Assert.assertEquals(
                mTestServer.getURL(RENDERED_PAGE),
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338972492
    public void testSwipeNavigateOnNativePage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes("Navigation.DuringGesture.NavStarted", false, 2)
                        .expectBooleanRecordTimes(
                                "Navigation.DuringGesture.NavStarted.3ButtonMode", false, 2)
                        .expectBooleanRecordTimes(
                                "Navigation.OnGestureStart.NavigationInProgress", false, 2)
                        .expectBooleanRecordTimes(
                                "Navigation.OnGestureStart.NavigationInProgress.3ButtonMode",
                                false,
                                2)
                        .build();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        assertNavigateOnSwipeFrom(LEFT_EDGE, UrlConstants.NTP_URL);
        assertNavigateOnSwipeFrom(RIGHT_EDGE, UrlConstants.RECENT_TABS_URL);
        histogramWatcher.assertExpected("Wrong histogram recording");

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Navigation.DuringGesture.NavStarted")
                        .expectNoRecords("Navigation.DuringGesture.NavStarted.3ButtonMode")
                        .expectNoRecords("Navigation.OnGestureStart.NavigationInProgress")
                        .expectNoRecords(
                                "Navigation.OnGestureStart.NavigationInProgress.3ButtonMode")
                        .build();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        ThreadUtils.runOnUiThreadBlocking(
                mActivityTestRule.getActivity().getOnBackPressedDispatcher()::onBackPressed);
        ThreadUtils.runOnUiThreadBlocking(
                mActivityTestRule.getActivity().getOnBackPressedDispatcher()::onBackPressed);
        histogramWatcher.assertExpected("Should not record when back is not triggered by swipe");
    }

    @Test
    @SmallTest
    public void testSwipeNavigateOnRenderedPage() {
        // TODO(crbug.com/40899221): Write a test variation running with
        //     ChromeFeatureList.BACK_FORWARD_TRANSITIONS enabled when the feature is completed.
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
        TabCreator tabCreator =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getActivity().getTabCreator(false));
        Tab newTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            UrlConstants.RECENT_TABS_URL, PageTransition.LINK),
                                    TabLaunchType.FROM_LINK,
                                    oldTab);
                        });
        Assert.assertEquals(newTab, currentTab());
        mNavUtils.swipeFromLeftEdge();

        // Assert that the new tab was closed and the old tab is the current tab again.
        CriteriaHelper.pollUiThread(() -> !newTab.isInitialized());
        Assert.assertNull(
                "Not supposed to trigger an animation when closing tab",
                mNavigationHandler.getTabOnBackGestureHandlerForTesting());
        Assert.assertEquals(oldTab, currentTab());
        Assert.assertEquals(
                "Chrome should remain in foreground",
                ActivityState.RESUMED,
                ApplicationStatus.getStateForActivity(mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    public void testSwipeAfterDestroy() {
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        ThreadUtils.runOnUiThreadBlocking(mNavigationHandler::destroy);

        // |triggerUi| can be invoked by SwipeRefreshHandler on the rendered
        // page. Make sure this won't crash after the handler(and also
        // handler action delegate) is destroyed.
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mNavigationHandler.triggerUi(
                                        BackGestureEventSwipeEdge.LEFT,
                                        NavigationHandler.TriggerUiCallSource.ON_SCROLL)));

        // Just check we're still on the same URL.
        Assert.assertEquals(
                mTestServer.getURL(RENDERED_PAGE),
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    @Test
    @SmallTest
    public void testSwipeAfterTabDestroy() {
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        ThreadUtils.runOnUiThreadBlocking(currentTab()::destroy);

        // |triggerUi| can be invoked by SwipeRefreshHandler on the rendered
        // page. Make sure this won't crash after the current tab is destroyed.
        Assert.assertFalse(
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mNavigationHandler.triggerUi(
                                        BackGestureEventSwipeEdge.LEFT,
                                        NavigationHandler.TriggerUiCallSource.ON_SCROLL)));
    }

    @Test
    @SmallTest
    public void testSwipeAfterDestroyActivity_NativePage() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        ThreadUtils.runOnUiThreadBlocking(mActivityTestRule.getActivity()::finish);

        // CompositorViewHolder dispatches motion events and invoke the handler's
        // |handleTouchEvent| on native pages. Make sure this won't crash the app after
        // the handler is destroyed.
        long eventTime = SystemClock.uptimeMillis();
        MotionEvent e =
                MotionEvent.obtain(
                        eventTime,
                        eventTime,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 10,
                        /* y= */ 100,
                        0);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getCompositorViewHolderForTesting()
                                .dispatchTouchEvent(e));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testEdgeSwipeIsNoopInTabSwitcher() throws TimeoutException {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        setTabSwitcherModeAndWait(true);
        mNavUtils.swipeFromLeftEdge();
        Assert.assertTrue(
                "Chrome should stay in tab switcher",
                mActivityTestRule.getActivity().isInOverviewMode());
        setTabSwitcherModeAndWait(false);
        Assert.assertEquals(
                "Current page should not change. ",
                UrlConstants.RECENT_TABS_URL,
                ChromeTabUtils.getUrlStringOnUiThread(currentTab()));
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testSwipeAndHoldOnNtp_EnterTabSwitcher() throws TimeoutException {
        // Clicking tab switcher button while swiping and holding the gesture navigation
        // bubble should reset the state and dismiss the UI.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mNavUtils.swipeFromEdgeAndHold(/* leftEdge= */ true);
        setTabSwitcherModeAndWait(true);
        Assert.assertFalse(
                "Navigation UI should be reset.",
                ThreadUtils.runOnUiThreadBlocking(mNavigationHandler::isActive));
    }

    @Test
    @SmallTest
    @EnableFeatures({UiAndroidFeatures.MIRROR_BACK_FORWARD_GESTURES_IN_RTL})
    public void testRtlUiMirrorsDirectionsWithFlagEnabled() {
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        setRtlForTesting(true);
        assertNavigateOnSwipeFrom(RIGHT_EDGE, mTestServer.getURL(RENDERED_PAGE));
        assertNavigateOnSwipeFrom(LEFT_EDGE, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    @DisableFeatures({UiAndroidFeatures.MIRROR_BACK_FORWARD_GESTURES_IN_RTL})
    public void testRtlUiMirrorsDirectionsWithFlagDisabled() {
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        setRtlForTesting(true);
        assertNavigateOnSwipeFrom(LEFT_EDGE, mTestServer.getURL(RENDERED_PAGE));
        assertNavigateOnSwipeFrom(RIGHT_EDGE, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }
}
