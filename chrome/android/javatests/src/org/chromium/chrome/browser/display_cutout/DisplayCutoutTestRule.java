// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.graphics.Rect;
import android.view.WindowManager.LayoutParams;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.rules.ExternalResource;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.TestThreadUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.concurrent.TimeoutException;

/**
 * Custom test rule for simulating a Display Cutout. This allows us to test display cutout
 * functionality without having a test device with a cutout.
 */
public class DisplayCutoutTestRule extends ExternalResource {
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
                Tab tab, final MonotonicObservableSupplier<Integer> browserCutoutModeSupplier) {
            DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                    new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(tab) {
                        @Override
                        public MonotonicObservableSupplier<Integer>
                                getBrowserDisplayCutoutModeSupplier() {
                            return browserCutoutModeSupplier;
                        }
                    };
            return new TestDisplayCutoutController(delegate);
        }

        /**
         * Creates a controller that observes the real {@link ActivityDisplayCutoutModeSupplier}
         * attached to the activity's window instead of a test-supplied one.
         */
        public static TestDisplayCutoutController createWithRealBrowserCutoutSupplier(Tab tab) {
            return new TestDisplayCutoutController(
                    new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(tab));
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
    public static final String TEST_PAGE =
            "/chrome/test/data/android/display_cutout/test_page.html";

    /** The default test timeout. */
    private static final int TEST_TIMEOUT = 5000;

    /** The {@link ChromeActivity} running the test. */
    private ChromeActivity mActivity;

    /** The {@link DisplayCutoutController} to test. */
    private TestDisplayCutoutController mTestController;

    /** Tracks whether the current tab is fullscreen. */
    private boolean mIsTabFullscreen;

    /** The {@link Tab} we are running the test in. */
    private Tab mTab;

    /** The {@link FullscreenManager.Observer} observing fullscreen mode. */
    private FullscreenManager.Observer mListener;

    /**
     * Attaches a {@link TestDisplayCutoutController} to {@code tab} and starts tracking fullscreen
     * state in {@code activity}.
     */
    public void attach(ChromeActivity activity, Tab tab) {
        assert mListener == null : "attach() should be called once per test";
        mActivity = activity;
        mTab = tab;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setDisplayCutoutController(TestDisplayCutoutController.create(tab, null));
                    mListener = new FullscreenToggleObserver();
                    mActivity.getFullscreenManager().addObserver(mListener);
                });
    }

    @Override
    protected void after() {
        if (mListener == null) return;
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getFullscreenManager().removeObserver(mListener));
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

    /** Wait for the main frame to report a positive top safe-area inset. */
    public void waitForSafeAreaTopInset() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                getAppliedSafeArea().top,
                                Matchers.greaterThan(TEST_SAFE_AREA_WITHOUT_CUTOUT.top));
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
        // A subsequently call to exitFullscreen() seems not to work without this, at least for
        // android-13 emulators.
        TestThreadUtils.sleep(500);
    }
}
