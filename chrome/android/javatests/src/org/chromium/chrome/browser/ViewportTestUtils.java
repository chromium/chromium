// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.annotation.Px;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManagerTestUtils;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.test.transit.BaseCtaTransitTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.concurrent.TimeUnit;

/** Utility class providing viewport helpers for tests. */
public final class ViewportTestUtils {

    private boolean mSetupCalled;

    private final BaseCtaTransitTestRule mActivityTestRule;

    private static final int TEST_TIMEOUT = 10000;

    public ViewportTestUtils(BaseCtaTransitTestRule rule) {
        mActivityTestRule = rule;
    }

    /**
     * Sets up the test for browser controls.
     *
     * <p>Removes the three-second delay after a page load where browser controls are immovable so
     * the tests can move the browser controls.
     */
    public void setUpForBrowserControls() {
        ThreadUtils.runOnUiThreadBlocking(
                TabStateBrowserControlsVisibilityDelegate::disablePageLoadDelayForTests);
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mSetupCalled = true;
    }

    public void waitForBrowserControlsState(boolean shown) {
        int topControlsHeight = getTopControlsHeightPx();
        // The TopControlOffset is the offset of the controls top edge from the viewport top edge.
        // So fully shown the offset is 0, fully hidden it is -controls_height.
        int expectedOffset = shown ? 0 : -topControlsHeight;

        FullscreenManagerTestUtils.waitForBrowserControlsPosition(
                mActivityTestRule.getActivity(), expectedOffset);
    }

    public void hideBrowserControls() throws Throwable {
        Assert.assertTrue(mSetupCalled);

        // Ensure controls start fully shown. A new renderer initializes with controls hidden and
        // receives a signal to animate them to showing. Trying to hide the controls before that
        // animation has completed is flaky.
        waitForBrowserControlsState(/* shown= */ true);

        FullscreenManagerTestUtils.waitForPageToBeScrollable(mActivityTestRule.getActivityTab());
        waitForFramePresented();
        int initialPageHeight = getPageInnerHeightPx();
        int initialBottomMargin = getBottomMargins();
        FullscreenManagerTestUtils.waitForBrowserControlsToBeMoveable(
                mActivityTestRule.getActivity(), /* showControls= */ false);

        // Also wait for the browser controls to resize Blink before returning.
        int finalHeight = initialPageHeight + getTopControlsHeightPx() + initialBottomMargin;
        waitForExpectedPageHeight(finalHeight);
    }

    public void waitForExpectedPageHeight(double expectedPageHeight) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        int curHeight = getPageInnerHeightPx();
                        // Allow 1px delta to account for device scale factor rounding.
                        Criteria.checkThat(
                                (double) curHeight,
                                Matchers.closeTo(expectedPageHeight, /* error= */ 1.0));
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    public void waitForExpectedVisualViewportHeight(double expectedHeight) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        double curHeight = getVisualViewportHeightPx();
                        // Allow 1px delta to account for device scale factor rounding.
                        Criteria.checkThat(
                                curHeight, Matchers.closeTo(expectedHeight, /* error= */ 1.0));
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    // Force generating a new compositor frame from the renderer and wait until
    // its presented on screen.
    public void waitForFramePresented() throws Throwable {
        final CallbackHelper ch = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getWebContents()
                            .getMainFrame()
                            .insertVisualStateCallback(result -> ch.notifyCalled());
                });

        ch.waitForNext(TEST_TIMEOUT, TimeUnit.SECONDS);

        // insertVisualStateCallback replies when a CompositorFrame is submitted. However, we want
        // to wait until the Viz process has received the new CompositorFrame so that the new frame
        // is available to a CopySurfaceRequest. Waiting for a second frame to be submitted
        // guarantees this since it cannot be sent until the first frame was ACKed by Viz.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getWebContents()
                            .getMainFrame()
                            .insertVisualStateCallback(result -> ch.notifyCalled());
                });

        ch.waitForNext(TEST_TIMEOUT, TimeUnit.SECONDS);
    }

    public double getDeviceScaleFactor() {
        return Coordinates.createFor(getWebContents()).getDeviceScaleFactor();
    }

    public int getTopControlsHeightPx() {
        BrowserControlsStateProvider browserControlsStateProvider =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        return browserControlsStateProvider.getTopControlsHeight();
    }

    public int getTopControlsHeightDp() {
        return (int) Math.floor(getTopControlsHeightPx() / getDeviceScaleFactor());
    }

    public @Px int getBottomMargins() {
        BrowserControlsStateProvider browserControlsStateProvider =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        return browserControlsStateProvider.getBottomControlsHeight()
                - browserControlsStateProvider.getBottomControlOffset();
    }

    public int getPageInnerHeightPx() throws Throwable {
        return (int)
                Math.round(
                        getDeviceScaleFactor()
                                * Integer.parseInt(
                                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                                getWebContents(), "window.innerHeight")));
    }

    public int getVisualViewportHeightPx() throws Throwable {
        return (int)
                Math.round(
                        getDeviceScaleFactor()
                                * Float.parseFloat(
                                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                                getWebContents(), "window.visualViewport.height")));
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getWebContents();
    }
}
