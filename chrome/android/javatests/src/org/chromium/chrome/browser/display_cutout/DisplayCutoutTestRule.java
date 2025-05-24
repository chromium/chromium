// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.graphics.Rect;
import android.os.Build;
import android.view.WindowManager.LayoutParams;

import androidx.annotation.RequiresApi;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.TestThreadUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Custom test rule for simulating a Display Cutout. This allows us to test display cutout
 * functionality without having a test device with a cutout.
 *
 * @param <T> The type of {@link ChromeActivity} to use for the test.
 */
@RequiresApi(Build.VERSION_CODES.P)
public class DisplayCutoutTestRule<T extends ChromeActivity> extends ChromeActivityTestRule<T> {
    /** These are the two test safe areas with and without the test cutout. */
    public static final Rect TEST_SAFE_AREA_WITH_CUTOUT = new Rect(10, 20, 30, 40);

    public static final Rect TEST_SAFE_AREA_WITHOUT_CUTOUT = new Rect(0, 0, 0, 0);

    /** These are used for testing different device dip scales. */
    public static final Rect TEST_SAFE_AREA_WITH_CUTOUT_HIGH_DIP = new Rect(4, 8, 12, 16);

    public static final float TEST_HIGH_DIP_SCALE = 2.5f;

    /** These are the different possible viewport fit values. */
    public static final String VIEWPORT_FIT_AUTO = "auto";

    public static final String VIEWPORT_FIT_CONTAIN = "contain";
    public static final String VIEWPORT_FIT_COVER = "cover";

    /** This class has polyfills for Android P+ system apis. */
    public static final class TestDisplayCutoutController extends DisplayCutoutController {
        private boolean mDeviceHasCutout = true;
        private float mDipScale = 1;

        public static TestDisplayCutoutController create(
                Tab tab, final ObservableSupplier<Integer> browserCutoutModeSupplier) {
            DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                    new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(tab) {
                        @Override
                        public ObservableSupplier<Integer> getBrowserDisplayCutoutModeSupplier() {
                            return browserCutoutModeSupplier;
                        }
                    };
            return new TestDisplayCutoutController(delegate);
        }

        private TestDisplayCutoutController(DisplayCutoutController.Delegate delegate) {
            super(delegate);
        }

        @Override
        protected void setWindowAttributes(LayoutParams attributes) {
            super.setWindowAttributes(attributes);

            // Apply insets based on new layout mode.
            if (getLayoutInDisplayCutoutMode()
                            == LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
                    && mDeviceHasCutout) {
                onSafeAreaChanged(new Rect(TEST_SAFE_AREA_WITH_CUTOUT));
            } else {
                onSafeAreaChanged(new Rect(TEST_SAFE_AREA_WITHOUT_CUTOUT));
            }
        }

        public int getLayoutInDisplayCutoutMode() {
            return getWindowAttributes().layoutInDisplayCutoutMode;
        }

        @Override
        protected float getDipScale() {
            return mDipScale;
        }

        public void setDipScale(float scale) {
            mDipScale = scale;
        }

        public void setDeviceHasCutout(boolean hasCutout) {
            mDeviceHasCutout = hasCutout;
        }
    }

    /** Listens to fullscreen tab events and tracks the fullscreen state of the tab. */
    private class FullscreenToggleObserver implements FullscreenManager.Observer {
        @Override
        public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
            mIsTabFullscreen = true;
        }

        @Override
        public void onExitFullscreen(Tab tab) {
            mIsTabFullscreen = false;
        }
    }

    /** The test page with the display cutout harness. */
    private static final String DEFAULT_TEST_PAGE =
            "/chrome/test/data/android/display_cutout/test_page.html";

    /** The default test timeout. */
    private static final int TEST_TIMEOUT = 5000;

    /** The embedded test HTTP server that serves the test page. */
    private EmbeddedTestServer mTestServer;

    /** The {@link DisplayCutoutController} to test. */
    private TestDisplayCutoutController mTestController;

    /** Tracks whether the current tab is fullscreen. */
    private boolean mIsTabFullscreen;

    /** The {@link Tab} we are running the test in. */
    private Tab mTab;

    /** The {@link FullscreenManager.Observer} observing fullscreen mode. */
    private FullscreenManager.Observer mListener;

    public DisplayCutoutTestRule(Class<T> activityClass) {
        super(activityClass);
    }

    @Override
    protected void before() throws Throwable {
        super.before();

        startActivity();
        mTab = getActivity().getActivityTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setDisplayCutoutController(TestDisplayCutoutController.create(mTab, null));
                    mListener = new FullscreenToggleObserver();
                    getActivity().getFullscreenManager().addObserver(mListener);
                });
        loadUrl(getTestURL());
    }

    protected void startActivity() {
        ChromeTabbedActivityTestRule rule = new ChromeTabbedActivityTestRule();
        rule.startMainActivityOnBlankPage();
        setActivity((T) rule.getActivity());
    }

    @Override
    protected void after() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> getActivity().getFullscreenManager().removeObserver(mListener));
        super.after();
    }

    protected String getTestURL() {
        if (mTestServer == null) {
            mTestServer =
                    EmbeddedTestServer.createAndStartServer(
                            InstrumentationRegistry.getInstrumentation().getContext());
        }
        return mTestServer.getURL(DEFAULT_TEST_PAGE);
    }

    protected void setDisplayCutoutController(TestDisplayCutoutController controller) {
        mTestController = controller;
        DisplayCutoutTabHelper.initForTesting(mTab, mTestController);
    }

    /** Set a simulated dip scale for this device. */
    public void setDipScale(float scale) {
        mTestController.setDipScale(scale);
    }

    /** Change whether this device has a display cutout. */
    public void setDeviceHasCutout(boolean hasCutout) {
        mTestController.setDeviceHasCutout(hasCutout);
    }

    /** Get the applied layout in display cutout mode. */
    public int getLayoutInDisplayCutoutMode() {
        return mTestController.getLayoutInDisplayCutoutMode();
    }

    /** Enter fullscreen and wait for the tab to go fullscreen. */
    public void enterFullscreen() throws TimeoutException {
        enterFullscreenUsingButton("fullscreen");
    }

    /** Exit fullscreen and wait for the tab to exit fullscreen. */
    public void exitFullscreen() {
        JavaScriptUtils.executeJavaScript(mTab.getWebContents(), "document.webkitExitFullscreen()");

        CriteriaHelper.pollUiThread(
                () -> !mIsTabFullscreen, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Wait for the main frame to have a certain applied safe area. */
    public void waitForSafeArea(Rect expected) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(getAppliedSafeArea(), Matchers.is(expected));
                    } catch (TimeoutException ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Wait for the sub frame to have a certain applied safe area. */
    public void waitForSafeAreaOnSubframe(Rect expected) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(getAppliedSafeAreaOnSubframe(), Matchers.is(expected));
                    } catch (TimeoutException ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Wait for the tab to have a certain {@layoutInDisplayCutoutMode. */
    public void waitForLayoutInDisplayCutoutMode(int expected) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mTestController.getLayoutInDisplayCutoutMode(), Matchers.is(expected));
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Enter fullscreen on the subframe and wait for the tab to go fullscreen. */
    public void enterFullscreenOnSubframe() throws TimeoutException {
        enterFullscreenUsingButton("subframefull");
    }

    /** Get the applied safe areas from the main frame. */
    public Rect getAppliedSafeArea() throws TimeoutException {
        return getSafeAreaUsingJavaScript("getSafeAreas()");
    }

    /** Get the applied safe areas from the child frame. */
    public Rect getAppliedSafeAreaOnSubframe() throws TimeoutException {
        return getSafeAreaUsingJavaScript("frameWindow.getSafeAreas()");
    }

    /** Set the viewport-fit meta tag on the main frame. */
    public void setViewportFit(String value) throws TimeoutException {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mTab.getWebContents(), "setViewportFit('" + value + "')");
    }

    /** Set the viewport-fit value using internal APIs. */
    public void setViewportFitInternal(@WebContentsObserver.ViewportFitType int value) {
        ThreadUtils.runOnUiThreadBlocking(() -> mTestController.setViewportFit(value));
    }

    /** Get the safe area using JS and parse the JSON result to a Rect. */
    private Rect getSafeAreaUsingJavaScript(String code) throws TimeoutException {
        try {
            String result =
                    JavaScriptUtils.executeJavaScriptAndWaitForResult(mTab.getWebContents(), code);
            JSONObject jsonResult = new JSONObject(result);
            return new Rect(
                    jsonResult.getInt("left"),
                    jsonResult.getInt("top"),
                    jsonResult.getInt("right"),
                    jsonResult.getInt("bottom"));
        } catch (JSONException e) {
            e.printStackTrace();
            Assert.fail("Failed to get safe area");
            return new Rect(0, 0, 0, 0);
        }
    }

    /**
     * Enter fullscreen by clicking on the supplied button and wait for the tab to go fullscreen.
     */
    private void enterFullscreenUsingButton(String id) throws TimeoutException {
        Assert.assertTrue(DOMUtils.clickNode(mTab.getWebContents(), id));

        CriteriaHelper.pollUiThread(
                () -> mIsTabFullscreen, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        // A subsequently call to exitFullscreen() seems not to work without this, at least for android-13 emulators.
        TestThreadUtils.sleep(500);
    }
}
