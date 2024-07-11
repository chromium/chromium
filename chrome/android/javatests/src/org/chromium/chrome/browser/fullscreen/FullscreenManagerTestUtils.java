// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.os.SystemClock;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Utility methods for testing the {@link BrowserControlsManager}. */
public class FullscreenManagerTestUtils {
    /**
     * Scrolls the underlying web page to show or hide the browser controls.
     *
     * @param testRule The test rule for the currently running test.
     * @param show Whether the browser controls should be shown.
     */
    public static void scrollBrowserControls(ChromeActivityTestRule testRule, boolean show) {
        BrowserControlsStateProvider browserControlsStateProvider =
                testRule.getActivity().getBrowserControlsManager();
        int browserControlsHeight = browserControlsStateProvider.getTopControlsHeight();

        waitForPageToBeScrollable(testRule.getActivity().getActivityTab());

        float dragX = 50f;
        // Use a larger scroll range than the height of the browser controls to ensure we overcome
        // the delay in a scroll start being sent.
        float dragStartY = browserControlsHeight * 3;
        float dragEndY = dragStartY - browserControlsHeight * 2;
        int expectedPosition = -browserControlsHeight;

        // The top back button toolbar will still be shown on automotive, even in fullscreen mode.
        if (show && !BuildInfo.getInstance().isAutomotive) {
            expectedPosition = 0;
            float tempDragStartY = dragStartY;
            dragStartY = dragEndY;
            dragEndY = tempDragStartY;
        }
        long downTime = SystemClock.uptimeMillis();
        TouchCommon.performDrag(
                testRule.getActivity(), dragX, dragX, dragStartY, dragEndY, 100, downTime);
        waitForBrowserControlsPosition(testRule, expectedPosition);
    }

    /**
     * Waits for the browser controls to reach the specified position.
     *
     * @param testRule The test rule for the currently running test.
     * @param position The desired top controls offset.
     */
    public static void waitForBrowserControlsPosition(
            ChromeActivityTestRule testRule, int position) {
        final BrowserControlsStateProvider browserControlsStateProvider =
                testRule.getActivity().getBrowserControlsManager();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            browserControlsStateProvider.getTopControlOffset(),
                            Matchers.is(position));
                });
    }

    /**
     * Waits for the base page to be scrollable.
     *
     * @param tab The current activity tab.
     */
    public static void waitForPageToBeScrollable(final Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            RenderCoordinates.fromWebContents(tab.getWebContents())
                                    .getContentHeightPixInt(),
                            Matchers.greaterThan(tab.getContentView().getHeight()));
                });
    }

    /**
     * Waits for the browser controls to be moveable by user gesture.
     *
     * <p>This function requires the browser controls to start fully visible. Then it ensures that
     * at some point the controls can be moved by user gesture. It will then fully cycle the top
     * controls to entirely hidden and back to fully shown.
     *
     * @param testRule The test rule for the currently running test.
     * @param tab The current activity tab.
     */
    public static void waitForBrowserControlsToBeMoveable(
            ChromeActivityTestRule testRule, final Tab tab) {
        waitForBrowserControlsPosition(testRule, 0);

        final CallbackHelper contentMovedCallback = new CallbackHelper();
        final BrowserControlsStateProvider browserControlsStateProvider =
                testRule.getActivity().getBrowserControlsManager();
        final float initialVisibleContentOffset =
                browserControlsStateProvider.getTopVisibleContentOffset();

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
                                    if (browserControlsStateProvider.getTopVisibleContentOffset()
                                            != initialVisibleContentOffset) {
                                        contentMovedCallback.notifyCalled();
                                        browserControlsStateProvider.removeObserver(this);
                                    }
                                }
                            });
                });

        float dragX = 50f;
        float dragStartY = tab.getView().getHeight() - 50f;

        for (int i = 0; i < 10; i++) {
            float dragEndY = dragStartY - browserControlsStateProvider.getTopControlsHeight();

            long downTime = SystemClock.uptimeMillis();
            TouchCommon.performDrag(
                    testRule.getActivity(), dragX, dragX, dragStartY, dragEndY, 100, downTime);

            try {
                contentMovedCallback.waitForCallback(0, 1, 500, TimeUnit.MILLISECONDS);
                scrollBrowserControls(testRule, false);
                scrollBrowserControls(testRule, true);
                return;
            } catch (TimeoutException e) {
                // Ignore and retry
            }
        }

        Assert.fail("Visible content never moved as expected.");
    }

    /** Disable any browser visibility overrides for testing. */
    public static void disableBrowserOverrides() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> BrowserStateBrowserControlsVisibilityDelegate.disableForTesting());
    }

    public static void fling(ChromeActivityTestRule testRule, final int vx, final int vy) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    testRule.getWebContents()
                            .getEventForwarder()
                            .startFling(
                                    SystemClock.uptimeMillis(),
                                    vx,
                                    vy,
                                    /* synthetic_scroll= */ false,
                                    /* prevent_boosting= */ false);
                });
    }
}
