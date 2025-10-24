// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import androidx.activity.BackEventCompat;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ViewportTestUtils;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.BackPressMetrics;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.BasicSmoothTransitionDelegate;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * End-to-end tests for default navigation transitions.
 *
 * <p>i.e. Dragging the current web contents back in history to reveal the previous web contents.
 * This suite is parameterized to run each test under the gestural navigation path as well as the
 * three-button navigation path.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-prefers-no-reduced-motion",
    // Resampling can make scroll offsets non-deterministic so turn it off.
    "disable-features=ResamplingScrollEvents",
    "hide-scrollbars"
})
@Batch(Batch.PER_CLASS)
// Native fence extension doesn't work properly on Android emulator
@DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/337886037")
@DisableIf.Build(supported_abis_includes = "x86_64", message = "https://crbug.com/337886037")
// TODO(crbug.com/423465927): Explore a better approach to make the
// existing tests run with the prewarm feature enabled.
@DisableFeatures({"Prewarm"})
public class NavigationTransitionsTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private EmbeddedTestServer mTestServer;

    private ViewportTestUtils mViewportTestUtils;
    private Bitmap mBitmap;

    private static final int TEST_TIMEOUT = 10000;

    private static final int NAVIGATION_MODE_THREE_BUTTON = 1;
    private static final int NAVIGATION_MODE_GESTURAL = 2;

    private ScreenshotCaptureTestHelper mScreenshotCaptureTestHelper;

    private Runnable mRelease;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet()
                            .value(NavigationTransitionsTest.NAVIGATION_MODE_THREE_BUTTON)
                            .name("ThreeButtonMode"),
                    new ParameterSet()
                            .value(NavigationTransitionsTest.NAVIGATION_MODE_GESTURAL)
                            .name("Gestural"));

    private final int mTestNavigationMode;
    private WebPageStation mPage;

    private static class ScreenshotCallback
            implements ScreenshotCaptureTestHelper.NavScreenshotCallback {

        @Override
        public Bitmap onAvailable(int navIndex, Bitmap bitmap, boolean requested) {
            Assert.assertEquals("Requested screenshot", mExpectRequested, requested);
            Bitmap overrideBitmap = null;
            if (requested) {
                // TODO(crbug.com/337886037) Capturing a screenshot currently fails in
                // emulators due to GPU issues. This override ensures we always return a
                // bitmap so that we can reliably run the test. This is ok since the current
                // tests don't pixel test the output (we do pixel test in other tests). Once
                // the emulator issues are fixed though it'd be better to remove this
                // override to perform a more realistic test.
                overrideBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
                overrideBitmap.eraseColor(Color.YELLOW);
            }
            if (mCallbackHelper != null) {
                mCallbackHelper.notifyCalled();
            }
            return overrideBitmap;
        }

        public CallbackHelper expectRequested(boolean expectRequested) {
            mCallbackHelper = new CallbackHelper();
            mExpectRequested = expectRequested;
            return mCallbackHelper;
        }

        private boolean mExpectRequested = true;
        private CallbackHelper mCallbackHelper;
    }

    private ScreenshotCallback mScreenshotCallback;

    public NavigationTransitionsTest(int navigationModeParam) {
        mTestNavigationMode = navigationModeParam;
    }

    @Before
    public void setUp() {
        mTestServer = mActivityTestRule.getTestServer();

        mScreenshotCaptureTestHelper = new ScreenshotCaptureTestHelper();

        mPage = mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        BackPressManager backPressManager =
                mActivityTestRule.getActivity().getBackPressManagerForTesting();

        boolean threeButtonMode = mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GestureNavigationTestUtils utils =
                            new GestureNavigationTestUtils(mActivityTestRule::getActivity);
                    utils.enableGestureNavigationForTesting(threeButtonMode);
                });
        backPressManager.setIsGestureNavEnabledSupplier(() -> !threeButtonMode);

        mScreenshotCallback = new ScreenshotCallback();
        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(mScreenshotCallback);
        mViewportTestUtils = new ViewportTestUtils(mActivityTestRule);
        mViewportTestUtils.setUpForBrowserControls();
        GestureNavigationUtils.setMinRequiredPhysicalRamMbForTesting(0);
    }

    @After
    public void tearDown() {
        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(null);
        mBitmap = null;
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getWebContents();
    }

    private String getCurrentUrl() {
        return ChromeTabUtils.getUrlStringOnUiThread(mActivityTestRule.getActivityTab());
    }

    private void invokeNavigateGesture(@BackGestureEventSwipeEdge int edge) {
        assertThat(edge).isAnyOf(BackEventCompat.EDGE_LEFT, BackEventCompat.EDGE_RIGHT);

        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            float widthPx =
                    getWebContents().getWidth()
                            * Coordinates.createFor(getWebContents()).getDeviceScaleFactor();

            // Drag far enough to cause the back gesture to invoke.
            float fromEdgeStart = 5.0f;
            float dragDistance = widthPx - 10.0f;

            // if EDGE_LEFT
            float fromX = fromEdgeStart;
            float toX = fromEdgeStart + dragDistance;
            if (edge == BackEventCompat.EDGE_RIGHT) {
                fromX = widthPx - fromEdgeStart;
                toX = widthPx - fromEdgeStart - dragDistance;
            }

            assertThat(fromX).isGreaterThan(0);
            assertThat(fromX).isLessThan(widthPx);
            assertThat(toX).isGreaterThan(0);
            assertThat(toX).isLessThan(widthPx);

            // These are arbitrary values that drag far enough to cause the back gesture to invoke.
            //
            // Note: Prefer `performWallClockDrag()` over
            // `GestureNavigationUtils#swipeFromLeftEdge()` because in the renderer, we perform
            // coalescing of input events. `swipeFromLeftEdge()` dispatches all the events at once
            // and all the events can be coalesced into one single event, causing some of the visual
            // effect not being triggered.
            TouchCommon.performWallClockDrag(
                    mActivityTestRule.getActivity(),
                    fromX,
                    toX,
                    /* fromY= */ 400.0f,
                    /* toY= */ 400.0f,
                    /* duration= */ 2000,
                    /* dispatchIntervalMs= */ 60,
                    /* preventFling= */ true);
        } else {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        BackPressManager manager =
                                mActivityTestRule.getActivity().getBackPressManagerForTesting();
                        var backEvent = new BackEventCompat(0, 0, 0, edge);
                        manager.getCallback().handleOnBackStarted(backEvent);
                        backEvent = new BackEventCompat(1, 0, .8f, edge);
                        manager.getCallback().handleOnBackProgressed(backEvent);
                        manager.getCallback().handleOnBackPressed();
                    });
        }
    }

    private Runnable performNavigationTransitionAndHold(
            String expectedUrl, @BackGestureEventSwipeEdge int edge) {
        assertThat(edge).isAnyOf(BackEventCompat.EDGE_LEFT, BackEventCompat.EDGE_RIGHT);
        final float widthPx =
                getWebContents().getWidth()
                        * Coordinates.createFor(getWebContents()).getDeviceScaleFactor();
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            // Drag far enough to cause the back gesture to invoke.
            float fromEdgeStart = 5.0f;
            float dragDistance = widthPx / 2;

            final float fromX =
                    edge == BackEventCompat.EDGE_LEFT ? fromEdgeStart : widthPx - fromEdgeStart;
            final float toX =
                    edge == BackEventCompat.EDGE_LEFT
                            ? fromEdgeStart + dragDistance
                            : widthPx - fromEdgeStart - dragDistance;

            assertThat(fromX).isGreaterThan(0);
            assertThat(fromX).isLessThan(widthPx);
            assertThat(toX).isGreaterThan(0);
            assertThat(toX).isLessThan(widthPx);

            long downTime = TimeUtils.currentTimeMillis();
            TouchCommon.dragStart(mActivityTestRule.getActivity(), fromX, 400.0f, downTime);

            TouchCommon.dragTo(
                    mActivityTestRule.getActivity(), fromX, toX, 400.0f, 400.0f, 100, downTime);
            mRelease =
                    () -> {
                        TouchCommon.dragEnd(mActivityTestRule.getActivity(), toX, 400.0f, downTime);
                    };
        } else {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        BackPressManager manager =
                                mActivityTestRule.getActivity().getBackPressManagerForTesting();
                        var backEvent = new BackEventCompat(0, 0, 0, edge);
                        manager.getCallback().handleOnBackStarted(backEvent);
                        backEvent = new BackEventCompat(widthPx / 2, 0, .8f, edge);
                        manager.getCallback().handleOnBackProgressed(backEvent);
                    });
            mRelease =
                    () -> {
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    BackPressManager manager =
                                            mActivityTestRule
                                                    .getActivity()
                                                    .getBackPressManagerForTesting();
                                    manager.getCallback().handleOnBackPressed();
                                });
                    };
        }
        return mRelease;
    }

    private void performNavigationTransition(
            String expectedUrl, @BackGestureEventSwipeEdge int edge) {
        Tab tab = mActivityTestRule.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(
                tab,
                expectedUrl,
                () -> {
                    invokeNavigateGesture(edge);
                });
    }

    private void waitForTransitionFinished() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        boolean hasTransition =
                                getWebContents().getCurrentBackForwardTransitionStage()
                                        != AnimationStage.NONE;
                        Criteria.checkThat(hasTransition, Matchers.is(false));
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void waitForModalDialogShown() {
        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        Criteria.checkThat(getDialogManager().isShowing(), Matchers.is(true));
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void runJavaScriptOnTab(String script) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getWebContents().evaluateJavaScriptForTests(script, null);
                });
    }

    private ModalDialogManager getDialogManager() {
        return mActivityTestRule.getActivity().getWindowAndroid().getModalDialogManager();
    }

    private int numSuspendedDialogs(@ModalDialogType int dialogType) {
        var dialogs = getDialogManager().getPendingDialogsForTest(dialogType);
        if (dialogs == null) return 0;
        return dialogs.size();
    }

    private void waitForNumSuspendedDialogs(@ModalDialogType int dialogType, int numSuspended) {
        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                numSuspendedDialogs(dialogType), Matchers.is(numSuspended));
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void loadUrlAndWaitForScreenshotCallback(String url, CallbackHelper helper)
            throws TimeoutException {
        mActivityTestRule.loadUrl(url);
        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());
        helper.waitForNext();
    }

    /**
     * Basic smoke test of transition back navigation.
     *
     * <p>Ensures that the transition gesture can be used to successfully navigate back in session
     * history.
     */
    @Test
    @MediumTest
    public void smokeTest() throws Throwable {
        if (mTestNavigationMode == NAVIGATION_MODE_GESTURAL
                && VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE) return;

        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/simple.html");
        var helper = mScreenshotCallback.expectRequested(true);
        loadUrlAndWaitForScreenshotCallback(url1, helper);
        loadUrlAndWaitForScreenshotCallback(url2, helper);
        loadUrlAndWaitForScreenshotCallback(url3, helper);

        HistogramWatcher.Builder builder = HistogramWatcher.newBuilder();
        HistogramWatcher watcher;

        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            watcher =
                    builder.expectBooleanRecord("GestureNavigation.Activated2", false)
                            .expectBooleanRecord("GestureNavigation.Completed2", false)
                            .build();
        } else {
            watcher =
                    builder.expectIntRecord(
                                    "Android.PredictiveGestureNavigation",
                                    BackPressMetrics.PredictiveGestureNavPhase.ACTIVATED)
                            .expectIntRecord(
                                    "Android.PredictiveGestureNavigation",
                                    BackPressMetrics.PredictiveGestureNavPhase.COMPLETED)
                            .expectIntRecord(
                                    "Android.PredictiveGestureNavigation.WithTransition",
                                    BackPressMetrics.PredictiveGestureNavPhase.ACTIVATED)
                            .expectIntRecord(
                                    "Android.PredictiveGestureNavigation.WithTransition",
                                    BackPressMetrics.PredictiveGestureNavPhase.COMPLETED)
                            .build();
        }

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        helper =
                mScreenshotCallback.expectRequested(
                        mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON);
        // Perform a back gesture transition from the left edge.
        performNavigationTransition(url2, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();
        helper.waitForNext();

        watcher.assertExpected();
        Assert.assertEquals(url2, getCurrentUrl());

        // Perform an edge gesture transition from the right edge. In three
        // button mode this goes forward, in gestural mode this goes back.
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            performNavigationTransition(url3, BackEventCompat.EDGE_RIGHT);
            waitForTransitionFinished();
            Assert.assertEquals(url3, getCurrentUrl());
        } else {
            performNavigationTransition(url1, BackEventCompat.EDGE_RIGHT);
            waitForTransitionFinished();
            Assert.assertEquals(url1, getCurrentUrl());
        }
        helper.waitForNext();
    }

    /**
     * Tests that when the user swipes from the right edge in OS gesture navigation mode, the tab
     * navigates forward with a preview if there is forward history.
     */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.RIGHT_EDGE_GOES_FORWARD_GESTURE_NAV)
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testRightEdgeGoesForwardInGestureNavMode() throws Throwable {
        // This test is only for gesture nav mode.
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) return;

        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/simple.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);
        mActivityTestRule.loadUrl(url3);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        // Swipe from the left edge (back nav) twice. url3 -> url2 -> url1.
        performNavigationTransition(url2, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();
        Assert.assertEquals(url2, getCurrentUrl());

        performNavigationTransition(url1, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();
        Assert.assertEquals(url1, getCurrentUrl());

        // Swipe from the right edge (forward nav) twice. url1 -> url2 -> url3.
        performNavigationTransition(url2, BackEventCompat.EDGE_RIGHT);
        waitForTransitionFinished();
        Assert.assertEquals(url2, getCurrentUrl());

        performNavigationTransition(url3, BackEventCompat.EDGE_RIGHT);
        waitForTransitionFinished();
        Assert.assertEquals(url3, getCurrentUrl());

        // Swipe from the left edge (back nav) once and then from the right edge (forward nav) once.
        // url3 -> url2 -> url3.
        performNavigationTransition(url2, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();
        Assert.assertEquals(url2, getCurrentUrl());

        performNavigationTransition(url3, BackEventCompat.EDGE_RIGHT);
        waitForTransitionFinished();
        Assert.assertEquals(url3, getCurrentUrl());
    }

    /**
     * Test semantic forward and backward swipes when directions are mirrored due to an RTL UI
     * direction.
     */
    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.RIGHT_EDGE_GOES_FORWARD_GESTURE_NAV,
    })
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testRightEdgeGoesForwardInGestureNavModeInRTL() throws Throwable {
        // This test is only for gesture nav mode.
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) return;

        setRtlForTesting(true);

        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/simple.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);
        mActivityTestRule.loadUrl(url3);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        // Swipe from the right edge (back nav) twice. url3 -> url2 -> url1.
        performNavigationTransition(url2, BackEventCompat.EDGE_RIGHT);
        waitForTransitionFinished();
        Assert.assertEquals(url2, getCurrentUrl());

        performNavigationTransition(url1, BackEventCompat.EDGE_RIGHT);
        waitForTransitionFinished();
        Assert.assertEquals(url1, getCurrentUrl());

        // Swipe from the left edge (forward nav) twice. url1 -> url2 -> url3.
        performNavigationTransition(url2, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();
        Assert.assertEquals(url2, getCurrentUrl());

        performNavigationTransition(url3, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();
        Assert.assertEquals(url3, getCurrentUrl());

        // Swipe from the right edge (back nav) once and then from the left edge (forward nav) once.
        // url3 -> url2 -> url3.
        performNavigationTransition(url2, BackEventCompat.EDGE_RIGHT);
        waitForTransitionFinished();
        Assert.assertEquals(url2, getCurrentUrl());

        performNavigationTransition(url3, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();
        Assert.assertEquals(url3, getCurrentUrl());
    }

    /**
     * Test that input works after performing a back navigation.
     *
     * <p>Input is suppressed during the transition so this test ensures suppression is reset at the
     * end of the transition.
     */
    @Test
    @MediumTest
    public void testInputAfterBackTransition() throws Throwable {
        if (mTestNavigationMode == NAVIGATION_MODE_GESTURAL
                && VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE) return;

        var helper = mScreenshotCallback.expectRequested(true);
        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        loadUrlAndWaitForScreenshotCallback(url1, helper);
        loadUrlAndWaitForScreenshotCallback(url2, helper);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        // No screenshot on gesture mode when navigating back.
        helper =
                mScreenshotCallback.expectRequested(
                        mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON);
        performNavigationTransition(url1, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(),
                "window.numTouches = 0;"
                        + "window.addEventListener('touchstart', () => { window.numTouches++; });");

        // Wait a frame to ensure the touchhandler registration is pushed to the CC thread so that
        // it forwards touches to the main thread.
        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        TouchCommon.singleClickView(mActivityTestRule.getActivityTab().getContentView());

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        int numTouches =
                Integer.parseInt(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                getWebContents(), "window.numTouches"));

        Assert.assertEquals(1, numTouches);
        helper.waitForNext();
    }

    /**
     * Test that history navigation works when directions are mirrored due to an RTL UI direction.
     */
    @Test
    @MediumTest
    public void testBackNavInRTL() throws Throwable {
        if (mTestNavigationMode == NAVIGATION_MODE_GESTURAL
                && VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE) return;

        setRtlForTesting(true);

        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/simple.html");

        var helper = mScreenshotCallback.expectRequested(true);
        loadUrlAndWaitForScreenshotCallback(url1, helper);
        loadUrlAndWaitForScreenshotCallback(url2, helper);
        loadUrlAndWaitForScreenshotCallback(url3, helper);

        // No screenshot on gesture mode when navigating back.
        helper =
                mScreenshotCallback.expectRequested(
                        mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON);

        performNavigationTransition(url2, BackEventCompat.EDGE_RIGHT);
        waitForTransitionFinished();
        Assert.assertEquals(url2, getCurrentUrl());

        // Perform an edge gesture transition from the left edge (semantically
        // forward - since we're in RTL). In three button mode this goes
        // forward, in gestural mode this goes back (without a transition).
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            performNavigationTransition(url3, BackEventCompat.EDGE_LEFT);
            waitForTransitionFinished();
            Assert.assertEquals(url3, getCurrentUrl());
        } else {
            performNavigationTransition(url1, BackEventCompat.EDGE_LEFT);
            waitForTransitionFinished();
            Assert.assertEquals(url1, getCurrentUrl());
        }

        helper.waitForNext();
    }

    /**
     * Test that the top control is fully visible during a transition.
     *
     * <p>Ensures that the animation is started at the start of the transition.
     */
    @Test
    @MediumTest
    public void startBackNavWithTopControlHidden() throws Throwable {
        if (mTestNavigationMode == NAVIGATION_MODE_GESTURAL
                && VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE) return;

        // The top control's offset is -top_controls_height when controls are fully hidden, 0 when
        // fully shown.
        final AtomicInteger topControlOffsetDuringGesture = new AtomicInteger(Integer.MAX_VALUE);
        BrowserControlsStateProvider browserControlsStateProvider =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    browserControlsStateProvider.addObserver(
                            new BrowserControlsStateProvider.Observer() {
                                @Override
                                public void onControlsOffsetChanged(
                                        int topOffset,
                                        int topControlsMinHeightOffset,
                                        boolean topControlsMinHeightChanged,
                                        int bottomOffset,
                                        int bottomControlsMinHeightOffset,
                                        boolean bottomControlsMinHeightChanged,
                                        boolean requestNewFrame,
                                        boolean isVisibilityForced) {
                                    // Since in 3-button mode the gesture sequence is two seconds,
                                    // the top control must have started to show during the two
                                    // seconds.
                                    if (getWebContents().getCurrentBackForwardTransitionStage()
                                            == AnimationStage.OTHER) {
                                        topControlOffsetDuringGesture.set(topOffset);
                                    }
                                }
                            });
                });

        var helper = mScreenshotCallback.expectRequested(true);

        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        loadUrlAndWaitForScreenshotCallback(url1, helper);

        String url2 = mTestServer.getURL("/chrome/test/data/android/green_scroll.html");
        loadUrlAndWaitForScreenshotCallback(url2, helper);

        // No screenshot on gesture mode when navigating back.
        helper =
                mScreenshotCallback.expectRequested(
                        mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON);

        // Perform a back gesture transition.
        mViewportTestUtils.hideBrowserControls();
        performNavigationTransition(url1, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();

        Assert.assertEquals(url1, getCurrentUrl());

        Assert.assertTrue(
                topControlOffsetDuringGesture.get() > -mViewportTestUtils.getTopControlsHeightPx());
        mViewportTestUtils.waitForBrowserControlsState(/* shown= */ true);

        helper.waitForNext();
    }

    /**
     * Test that the modal dialogs are suspended during the transition but resumed after the
     * transition.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void alertSuspendedDuringTransition() throws Throwable {
        // Skip for three button mode because `TouchCommon.performWallClockDrag` blocks (it sleeps
        // between each touch event). It's okay to skip because the dialog suspension is navigation
        // mode agnostic.
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) return;

        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        final AtomicBoolean dialogQueuedToShow = new AtomicBoolean(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getDialogManager()
                            .addObserver(
                                    new ModalDialogManager.ModalDialogManagerObserver() {
                                        @Override
                                        public void onDialogAdded(PropertyModel model) {
                                            dialogQueuedToShow.set(true);
                                        }
                                    });
                });

        // No screenshot on gesture mode when navigating back.
        mScreenshotCallback.expectRequested(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackPressManager manager =
                            mActivityTestRule.getActivity().getBackPressManagerForTesting();
                    var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackStarted(backEvent);
                });
        runJavaScriptOnTab("window.alert('during transition');");
        waitForNumSuspendedDialogs(ModalDialogType.TAB, 1);
        Assert.assertFalse(dialogQueuedToShow.get());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackPressManager manager =
                            mActivityTestRule.getActivity().getBackPressManagerForTesting();
                    var backEvent = new BackEventCompat(1, 0, 0.8f, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackProgressed(backEvent);
                    manager.getCallback().handleOnBackPressed();
                });

        waitForTransitionFinished();
        Assert.assertEquals(0, numSuspendedDialogs(ModalDialogType.TAB));
        Assert.assertFalse(dialogQueuedToShow.get());

        // After the transition, dialogs are resumed.
        runJavaScriptOnTab("window.alert('after transition');");
        waitForModalDialogShown();
        Assert.assertTrue(dialogQueuedToShow.get());
    }

    /**
     * Test that the modal dialogs are suspended during the transition but resumed after the
     * transition is cancelled.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void alertResumedAfterGestureCancelled() throws Throwable {
        // TouchCommon.java doesn't have a "cancel gesture".
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) return;

        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);

        final AtomicBoolean dialogQueuedToShow = new AtomicBoolean(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getDialogManager()
                            .addObserver(
                                    new ModalDialogManager.ModalDialogManagerObserver() {
                                        @Override
                                        public void onDialogAdded(PropertyModel model) {
                                            dialogQueuedToShow.set(true);
                                        }
                                    });
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackPressManager manager =
                            mActivityTestRule.getActivity().getBackPressManagerForTesting();
                    var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackStarted(backEvent);
                });
        runJavaScriptOnTab("window.alert('shown after transition');");
        waitForNumSuspendedDialogs(ModalDialogType.TAB, 1);
        Assert.assertFalse(dialogQueuedToShow.get());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackPressManager manager =
                            mActivityTestRule.getActivity().getBackPressManagerForTesting();
                    var backEvent = new BackEventCompat(1, 0, .8f, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackProgressed(backEvent);
                    manager.getCallback().handleOnBackCancelled();
                });
        waitForTransitionFinished();
        Assert.assertEquals(0, numSuspendedDialogs(ModalDialogType.TAB));

        // After the transition is finished, the dialog is resumed.
        waitForModalDialogShown();
        Assert.assertTrue(dialogQueuedToShow.get());
    }

    /**
     * Test that it doesn't crash when handleOnBackProgressed is called without handleOnBackStarted.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testNoCrashWhenGestureIsNotInProgress() throws TimeoutException {
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            return;
        }
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/simple.html");

        var helper = mScreenshotCallback.expectRequested(true);
        loadUrlAndWaitForScreenshotCallback(url1, helper);
        loadUrlAndWaitForScreenshotCallback(url2, helper);
        loadUrlAndWaitForScreenshotCallback(url3, helper);

        // Perform a back gesture transition from the left edge.
        // No screenshot on gesture mode when navigating back.
        helper =
                mScreenshotCallback.expectRequested(
                        mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON);
        performNavigationTransition(url2, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();

        Assert.assertEquals(url2, getCurrentUrl());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackPressManager manager =
                            mActivityTestRule.getActivity().getBackPressManagerForTesting();
                    var backEvent = new BackEventCompat(1, 0, .8f, BackEventCompat.EDGE_LEFT);
                    // BackPressManager will prevent this from triggering because of no active
                    // handler.
                    manager.getCallback().handleOnBackProgressed(backEvent);
                    manager.getCallback().handleOnBackPressed();
                });

        ChromeTabUtils.waitForTabPageLoaded(mActivityTestRule.getActivityTab(), url1);

        helper.waitForNext();
    }

    /** Test that it doesn't crash when the edge is somehow changed in the mid of swipe gesture. */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @DisabledTest(message = "crbug.com/377320122")
    public void testNoCrashWhenGestureEdgeIsChangedUnexpectedly() throws TimeoutException {
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            return;
        }
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/simple.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);
        mActivityTestRule.loadUrl(url3);
        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        // Perform a back gesture transition from the left edge.
        performNavigationTransition(url2, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();

        Assert.assertEquals(url2, getCurrentUrl());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackPressManager manager =
                            mActivityTestRule.getActivity().getBackPressManagerForTesting();
                    var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackStarted(backEvent);
                    backEvent = new BackEventCompat(1, 0, .8f, BackEventCompat.EDGE_LEFT);
                    manager.getCallback().handleOnBackProgressed(backEvent);
                    backEvent = new BackEventCompat(2, 0, .9f, BackEventCompat.EDGE_RIGHT);
                    manager.getCallback().handleOnBackProgressed(backEvent);
                    manager.getCallback().handleOnBackPressed();
                });

        waitForTransitionFinished();

        Assert.assertEquals(url1, getCurrentUrl());
    }

    /** Test that it falls back to fallback screenshot when navigating between native pages. */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/379861088")
    public void testNavigateBetweenNativePages() throws TimeoutException {
        if (mTestNavigationMode == NAVIGATION_MODE_GESTURAL
                && VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE) return;

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        CallbackHelper callbackHelper = new CallbackHelper();

        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(
                new ScreenshotCaptureTestHelper.NavScreenshotCallback() {
                    @Override
                    public Bitmap onAvailable(int navIndex, Bitmap bitmap, boolean requested) {
                        mBitmap = bitmap;
                        callbackHelper.notifyCalled();
                        return mBitmap;
                    }
                });

        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        callbackHelper.waitForOnly();
        Assert.assertNull("Should capture a null when navigating between native pages", mBitmap);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/398140569")
    public void testSwipeBackToNativeBookmarksPageWithTransition() throws InterruptedException {
        final Tab tab = mActivityTestRule.getActivityTab();
        mActivityTestRule.loadUrl("chrome-native://bookmarks/folder/0");
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/blue.html"));

        //         No screenshot on gesture mode when navigating back.
        mScreenshotCallback.expectRequested(mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON);
        Runnable release =
                performNavigationTransitionAndHold(
                        "chrome-native://bookmarks/folder/0", BackEventCompat.EDGE_LEFT);
        Assert.assertEquals(
                "Back forward transition not invoked yet",
                AnimationStage.OTHER,
                tab.getWebContents().getCurrentBackForwardTransitionStage());

        release.run();
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

    /** Tests that the favicon bitmap when navigating back to a native page is not null. */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/392629755")
    public void testFallbackUxFaviconForNativePages() throws TimeoutException {
        // This test is for both 3-button and gesture nav mode.
        if (mTestNavigationMode == NAVIGATION_MODE_GESTURAL
                && VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE) return;
        // 1. Load NTP.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());
        // 2. Load url1.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        mActivityTestRule.loadUrl(url1);
        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        CallbackHelper callbackHelper = new CallbackHelper();
        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(
                (navIndex, bitmap, requested) -> {
                    mBitmap = bitmap;
                    callbackHelper.notifyCalled();
                    return mBitmap;
                });

        // 3. Swipe back to the NTP.
        performNavigationTransition(UrlConstants.NTP_URL, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();
        Assert.assertEquals(UrlConstants.NTP_URL, getCurrentUrl());

        Assert.assertNotNull(
                "The favicon bitmap in the fallback ux of a native page should not be null.",
                mBitmap);
    }
}
