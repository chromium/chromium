// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.Bitmap;
import android.graphics.Color;

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
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.List;

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
    // Resampling can make scroll offsets non-deterministic so turn it off.
    "disable-features=ResamplingScrollEvents"
})
@Batch(Batch.PER_CLASS)
public class NavigationTransitionsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

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
                        GestureNavigationUtils utils =
                                new GestureNavigationUtils(mActivityTestRule);
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

    private void invokeNavigateBack() {
        if (mTestNavigationMode == NAVIGATION_MODE_THREE_BUTTON) {
            // These are arbitrary values that drag far enough to cause the back gesture to invoke.
            TouchCommon.performWallClockDrag(
                    mActivityTestRule.getActivity(),
                    /* fromX= */ 5.0f,
                    /* toX= */ 1200.0f,
                    /* fromY= */ 400.0f,
                    /* toY= */ 400.0f,
                    /* duration= */ 2000,
                    /* dispatchIntervalMs= */ 60,
                    /* preventFling= */ true);
        } else {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        BackPressManager manager =
                                mActivityTestRule.getActivity().getBackPressManagerForTesting();
                        var backEvent = new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT);
                        manager.getCallback().handleOnBackStarted(backEvent);
                        backEvent = new BackEventCompat(1, 0, .8f, BackEventCompat.EDGE_LEFT);
                        manager.getCallback().handleOnBackProgressed(backEvent);
                        manager.getCallback().handleOnBackPressed();
                    });
        }
    }

    private void performBackNavigationTransition(String expectedUrl) {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(
                tab,
                expectedUrl,
                () -> {
                    invokeNavigateBack();
                });
        waitForTransitionFinished();
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

    /**
     * Basic smoke test of transition back navigation.
     *
     * <p>Ensures that the transition gesture can be used to successfully navigate back in session
     * history.
     */
    @Test
    @MediumTest
    public void smokeTest() throws Throwable {
        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());

        // Perform a back gesture transition
        performBackNavigationTransition(url1);

        Assert.assertEquals(url1, getCurrentUrl());
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
        // Put "blue.html" and then "green.html" in the session history.
        String url1 = mTestServer.getURL("/chrome/test/data/android/blue.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/green.html");
        mActivityTestRule.loadUrl(url1);
        mActivityTestRule.loadUrl(url2);

        WebContentsUtils.waitForCopyableViewInWebContents(getWebContents());
        performBackNavigationTransition(url1);

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
}
