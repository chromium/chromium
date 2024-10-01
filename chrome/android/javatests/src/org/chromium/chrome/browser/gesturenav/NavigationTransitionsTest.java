// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import androidx.activity.BackEventCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ViewportTestUtils;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.base.UiAndroidFeatures;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
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
    "enable-features=BackForwardTransitions,BackGestureRefactorAndroid",
    "force-prefers-no-reduced-motion",
    // Resampling can make scroll offsets non-deterministic so turn it off.
    "disable-features=ResamplingScrollEvents",
    "hide-scrollbars"
})
@Batch(Batch.PER_CLASS)
@DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/337886037")
@DisableIf.Build(supported_abis_includes = "x86_64", message = "https://crbug.com/337886037")
public class NavigationTransitionsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    private ViewportTestUtils mViewportTestUtils;

    private static final int TEST_TIMEOUT = 10000;

    private static final int NAVIGATION_MODE_THREE_BUTTON = 1;
    private static final int NAVIGATION_MODE_GESTURAL = 2;

    private ScreenshotCaptureTestHelper mScreenshotCaptureTestHelper;

    @ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet()
                            .value(NavigationTransitionsTest.NAVIGATION_MODE_THREE_BUTTON)
                            .name("ThreeButtonMode"),
                    new ParameterSet()
                            .value(NavigationTransitionsTest.NAVIGATION_MODE_GESTURAL)
                            .name("Gestural"));

    private int mTestNavigationMode;

    public NavigationTransitionsTest(int navigationModeParam) {
        mTestNavigationMode = navigationModeParam;
    }

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mScreenshotCaptureTestHelper = new ScreenshotCaptureTestHelper();

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        BackPressManager backPressManager =
                mActivityTestRule.getActivity().getBackPressManagerForTesting();

        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        GestureNavigationTestUtils utils =
                                new GestureNavigationTestUtils(mActivityTestRule);
                        utils.enableGestureNavigationForTesting();
                    });
            backPressManager.setIsGestureNavEnabledSupplier(() -> false);
        } else {
            backPressManager.setIsGestureNavEnabledSupplier(() -> true);
        }

        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(
                new ScreenshotCaptureTestHelper.NavScreenshotCallback() {
                    @Override
                    public Bitmap onAvailable(int navIndex, Bitmap bitmap, boolean requested) {
                        // TODO(crbug.com/337886037) Capturing a screenshot currently fails in
                        // emulators due to GPU issues. This override ensures we always return a
                        // bitmap so that we can reliably run the test. This is ok since the current
                        // tests don't pixel test the output (we do pixel test in other tests). Once
                        // the emulator issues are fixed though it'd be better to remove this
                        // override to perform a more realistic test.
                        Bitmap overrideBitmap =
                                Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
                        overrideBitmap.eraseColor(Color.YELLOW);
                        return overrideBitmap;
                    }
                });
        mViewportTestUtils = new ViewportTestUtils(mActivityTestRule);
        mViewportTestUtils.setUpForBrowserControls();
    }

    @After
    public void tearDown() {
        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(null);
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getActivity().getActivityTab().getWebContents();
    }

    private String getCurrentUrl() {
        return ChromeTabUtils.getUrlStringOnUiThread(
                mActivityTestRule.getActivity().getActivityTab());
    }

    private void invokeNavigateGesture(@BackGestureEventSwipeEdge int edge) {
        assert edge == BackEventCompat.EDGE_LEFT || edge == BackEventCompat.EDGE_RIGHT;
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            float width_px =
                    getWebContents().getWidth()
                            * Coordinates.createFor(getWebContents()).getDeviceScaleFactor();

            // Drag far enough to cause the back gesture to invoke.
            float fromEdgeStart = 5.0f;
            float dragDistance = width_px - 10.0f;

            // if EDGE_LEFT
            float fromX = fromEdgeStart;
            float toX = fromEdgeStart + dragDistance;
            if (edge == BackEventCompat.EDGE_RIGHT) {
                fromX = width_px - fromEdgeStart;
                toX = width_px - fromEdgeStart - dragDistance;
            }

            assert fromX > 0 && fromX < width_px;
            assert toX > 0 && toX < width_px;

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

    private void performNavigationTransition(
            String expectedUrl, @BackGestureEventSwipeEdge int edge) {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
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
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);
        mActivityTestRule.loadUrl(url3);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        // Perform a back gesture transition from the left edge.
        performNavigationTransition(url2, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();

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

        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());
        performNavigationTransition(url1, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(),
                "window.numTouches = 0;"
                        + "window.addEventListener('touchstart', () => { window.numTouches++; });");

        // Wait a frame to ensure the touchhandler registration is pushed to the CC thread so that
        // it forwards touches to the main thread.
        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        TouchCommon.singleClickView(
                mActivityTestRule.getActivity().getActivityTab().getContentView());

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        int numTouches =
                Integer.parseInt(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                getWebContents(), "window.numTouches"));

        Assert.assertEquals(1, numTouches);
    }

    /**
     * Test that history navigation works when directions are mirrored due to an RTL UI direction.
     */
    @Test
    @MediumTest
    @EnableFeatures({UiAndroidFeatures.MIRROR_BACK_FORWARD_GESTURES_IN_RTL})
    public void testBackNavInRTL() throws Throwable {
        if (mTestNavigationMode == NAVIGATION_MODE_GESTURAL
                && VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE) return;

        setRtlForTesting(true);

        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/simple.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);
        mActivityTestRule.loadUrl(url3);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());
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
                                        int bottomOffset,
                                        int bottomControlsMinHeightOffset,
                                        boolean needsAnimate,
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

        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green_scroll.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        // Perform a back gesture transition.
        mViewportTestUtils.hideBrowserControls();
        performNavigationTransition(url1, BackEventCompat.EDGE_LEFT);
        waitForTransitionFinished();

        Assert.assertEquals(url1, getCurrentUrl());

        Assert.assertTrue(
                topControlOffsetDuringGesture.get() > -mViewportTestUtils.getTopControlsHeightPx());
        mViewportTestUtils.waitForBrowserControlsState(/* shown= */ true);
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
}
