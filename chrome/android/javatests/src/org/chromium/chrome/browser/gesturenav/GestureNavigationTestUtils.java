// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.Point;
import android.util.DisplayMetrics;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Utility class providing gesture actions for tests. */
public final class GestureNavigationTestUtils {
    private final ChromeTabbedActivityTestRule mActivityTestRule;
    private final NavigationHandler mNavigationHandler;
    private final HistoryNavigationLayout mNavigationLayout;
    private float mEdgeWidthPx;

    public GestureNavigationTestUtils(ChromeTabbedActivityTestRule rule) {
        mActivityTestRule = rule;
        DisplayMetrics displayMetrics = new DisplayMetrics();
        mActivityTestRule
                .getActivity()
                .getWindowManager()
                .getDefaultDisplay()
                .getMetrics(displayMetrics);
        mEdgeWidthPx = displayMetrics.density * NavigationHandler.EDGE_WIDTH_DP;

        HistoryNavigationCoordinator coordinator = getNavigationCoordinator();
        mNavigationHandler = coordinator.getNavigationHandlerForTesting();
        mNavigationLayout = coordinator.getLayoutForTesting();
    }

    public HistoryNavigationLayout getLayout() {
        return mNavigationLayout;
    }

    public NavigationHandler getNavigationHandler() {
        return mNavigationHandler;
    }

    public void swipeFromLeftEdge() {
        swipeFromEdge(/* leftEdge= */ true);
    }

    public void swipeFromRightEdge() {
        swipeFromEdge(/* leftEdge= */ false);
    }

    public void swipeFromEdge(boolean leftEdge) {
        Point size = new Point();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        final float startx = leftEdge ? mEdgeWidthPx / 2 : size.x - mEdgeWidthPx / 2;
        final float endx = size.x / 2f;
        final float yMiddle = size.y / 2f;
        swipe(leftEdge, startx, endx, yMiddle);
    }

    public void swipeFromEdgeAndHold(boolean leftEdge) {
        Point size = new Point();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        final float startx = leftEdge ? mEdgeWidthPx / 2 : size.x - mEdgeWidthPx / 2;
        final float endx = size.x / 2f;
        final float yMiddle = size.y / 2f;
        swipeAndHold(leftEdge, startx, endx, yMiddle);
    }

    // Make an edge swipe too short to trigger the navigation.
    public void shortSwipeFromEdge(boolean leftEdge) {
        Point size = new Point();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);
        final float startx = leftEdge ? 0 : size.x;
        final float endx = leftEdge ? mEdgeWidthPx : size.x - mEdgeWidthPx;
        final float yMiddle = size.y / 2f;
        swipe(leftEdge, startx, endx, yMiddle);
    }

    /**
     * Enables gestural navigation for use in tests. Tests are responsible for manually injecting
     * simulated "back" gesture progress events.
     */
    public void enableGestureNavigationForTesting() {
        getNavigationCoordinator().forceFeatureEnabledForTesting();
    }

    private void swipe(boolean leftEdge, float startx, float endx, float y) {
        swipeAndHold(leftEdge, startx, endx, y);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNavigationHandler.release(true);
                });
    }

    private void swipeAndHold(boolean leftEdge, float startx, float endx, float y) {
        // # of pixels (of reasonally small value) which a finger moves across
        // per one motion event.
        final float distancePx = 6.0f;
        final float step = Math.signum(endx - startx) * distancePx;
        final int eventCounts = (int) ((endx - startx) / step);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNavigationHandler.onDown();
                    float nextx = startx + step;
                    for (int i = 0; i < eventCounts; i++, nextx += step) {
                        mNavigationHandler.onScroll(startx, -step, 0, nextx, y);
                    }
                });
    }

    private HistoryNavigationCoordinator getNavigationCoordinator() {
        TabbedRootUiCoordinator uiCoordinator =
                (TabbedRootUiCoordinator)
                        mActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        return uiCoordinator.getHistoryNavigationCoordinatorForTesting();
    }
}
