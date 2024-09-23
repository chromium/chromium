// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.view.Window;

import androidx.activity.BackEventCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.Type;
import org.chromium.ui.UiUtils;

/**
 * A utility class to record back press related histograms. TODO(crbug.com/41481803): Move other
 * histogram recording to this class.
 */
public class BackPressMetrics {
    private static final String EDGE_HISTOGRAM = "Android.BackPress.SwipeEdge";
    private static final String TAB_HISTORY_EDGE_HISTOGRAM =
            "Android.BackPress.SwipeEdge.TabHistoryNavigation";
    private static final String INTERCEPT_FROM_LEFT_HISTOGRAM =
            "Android.BackPress.Intercept.LeftEdge";
    private static final String INTERCEPT_FROM_RIGHT_HISTOGRAM =
            "Android.BackPress.Intercept.RightEdge";

    /**
     * @param type The {@link Type} of the back press handler.
     * @param edge The edge from which the gesture is swiped from {@link BackEventCompat}.
     */
    public static void recordBackPressFromEdge(@Type int type, int edge) {
        RecordHistogram.recordEnumeratedHistogram(EDGE_HISTOGRAM, edge, 2);

        String histogram =
                edge == BackEventCompat.EDGE_LEFT
                        ? INTERCEPT_FROM_LEFT_HISTOGRAM
                        : INTERCEPT_FROM_RIGHT_HISTOGRAM;
        RecordHistogram.recordEnumeratedHistogram(
                histogram, BackPressManager.getHistogramValue(type), Type.NUM_TYPES);
    }

    /**
     * @param edge The edge from which the gesture is swiped from {@link BackEventCompat}.
     */
    public static void recordTabNavigationSwipedFromEdge(int edge) {
        RecordHistogram.recordEnumeratedHistogram(TAB_HISTORY_EDGE_HISTOGRAM, edge, 2);
    }

    /**
     * @param didNavStartInBetween Whether a navigation is started during the gesture.
     * @param window The window in which the navigation gesture occurs.
     */
    public static void recordNavStatusDuringGesture(boolean didNavStartInBetween, Window window) {
        RecordHistogram.recordBooleanHistogram(
                "Navigation.DuringGesture.NavStarted", didNavStartInBetween);
        if (UiUtils.isGestureNavigationMode(window)) {
            RecordHistogram.recordBooleanHistogram(
                    "Navigation.DuringGesture.NavStarted.GestureMode", didNavStartInBetween);
        } else {
            RecordHistogram.recordBooleanHistogram(
                    "Navigation.DuringGesture.NavStarted.3ButtonMode", didNavStartInBetween);
        }
    }

    /**
     * @param isNavigationInProgress Whether a navigation has started and not finished yet.
     * @param window The window in which the navigation gesture occurs.
     */
    public static void recordNavStatusOnGestureStart(
            boolean isNavigationInProgress, Window window) {
        RecordHistogram.recordBooleanHistogram(
                "Navigation.OnGestureStart.NavigationInProgress", isNavigationInProgress);
        if (UiUtils.isGestureNavigationMode(window)) {
            RecordHistogram.recordBooleanHistogram(
                    "Navigation.OnGestureStart.NavigationInProgress.GestureMode",
                    isNavigationInProgress);
        } else {
            RecordHistogram.recordBooleanHistogram(
                    "Navigation.OnGestureStart.NavigationInProgress.3ButtonMode",
                    isNavigationInProgress);
        }
    }
}
