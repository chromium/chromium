// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.view.Window;

import androidx.activity.BackEventCompat;
import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.Type;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A utility class to record back press related histograms. TODO(crbug.com/41481803): Move other
 * histogram recording to this class.
 */
@NullMarked
public class BackPressMetrics {
    private static final String EDGE_HISTOGRAM = "Android.BackPress.SwipeEdge";
    private static final String TAB_HISTORY_EDGE_HISTOGRAM =
            "Android.BackPress.SwipeEdge.TabHistoryNavigation";
    private static final String INTERCEPT_FROM_LEFT_HISTOGRAM =
            "Android.BackPress.Intercept.LeftEdge";
    private static final String INTERCEPT_FROM_RIGHT_HISTOGRAM =
            "Android.BackPress.Intercept.RightEdge";
    private static final String INCORRECT_EDGE_SWIPE_HISTOGRAM =
            "Android.BackPress.IncorrectEdgeSwipe";
    private static final String INCORRECT_EDGE_SWIPE_COUNT_CHAINED_HISTOGRAM =
            "Android.BackPress.IncorrectEdgeSwipe.CountChained";
    private static final String BACK_FALSING_HISTOGRAM = "Android.BackPress.Backfalsing2";
    private static final String STRICT_BACK_FALSING_HISTOGRAM =
            "Android.BackPress.StrictBackfalsing";

    @IntDef({
        PredictiveGestureNavPhase.ACTIVATED,
        PredictiveGestureNavPhase.CANCELLED,
        PredictiveGestureNavPhase.COMPLETED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PredictiveGestureNavPhase {
        int ACTIVATED = 0;
        int CANCELLED = 1;
        int COMPLETED = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * Used to record when trying to capture a screenshot of native page view used for back forward
     * transition.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    @IntDef({
        CaptureNativeViewResult.NULL_WINDOW_ANDROID,
        CaptureNativeViewResult.VIEW_NOT_LAID_OUT,
        CaptureNativeViewResult.BETWEEN_NATIVE_PAGES,
        CaptureNativeViewResult.CAPTURE_SCREENSHOT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CaptureNativeViewResult {
        int NULL_WINDOW_ANDROID = 0;
        int VIEW_NOT_LAID_OUT = 1;
        int BETWEEN_NATIVE_PAGES = 2;
        int CAPTURE_SCREENSHOT = 3;

        int NUM_ENTRIES = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        NavigationDirection.FORWARD,
        NavigationDirection.BACKWARD,
        NavigationDirection.NEITHER
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NavigationDirection {
        int FORWARD = 0;
        int BACKWARD = 1;
        int NEITHER = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * @param navigationDirection The direction of the navigation.
     */
    public static void recordBackFalsing(@NavigationDirection int navigationDirection) {
        RecordHistogram.recordEnumeratedHistogram(
                BACK_FALSING_HISTOGRAM, navigationDirection, NavigationDirection.NUM_ENTRIES);
    }

    /**
     * @param navigationDirection The direction of the navigation.
     */
    public static void recordStrictBackFalsing(@NavigationDirection int navigationDirection) {
        RecordHistogram.recordEnumeratedHistogram(
                STRICT_BACK_FALSING_HISTOGRAM,
                navigationDirection,
                NavigationDirection.NUM_ENTRIES);
    }

    /**
     * @param edge The edge from which the gesture is swiped from {@link BackGestureEventSwipeEdge}.
     */
    public static void recordIncorrectEdgeSwipe(int edge) {
        RecordHistogram.recordEnumeratedHistogram(INCORRECT_EDGE_SWIPE_HISTOGRAM, edge, 2);
    }

    /**
     * @param count The consecutive number of incorrect edge swipes the user has performed.
     */
    public static void recordIncorrectEdgeSwipeCountChained(int count) {
        RecordHistogram.recordCount100Histogram(
                INCORRECT_EDGE_SWIPE_COUNT_CHAINED_HISTOGRAM, count);
    }

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

    /**
     * Record the phase when a gesture nav is started.
     *
     * @param transition If a back forward transition is triggered by this gesture.
     * @param phase The current {@link PredictiveGestureNavPhase}.
     */
    public static void recordPredictiveGestureNav(
            boolean transition, @PredictiveGestureNavPhase int phase) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PredictiveGestureNavigation",
                phase,
                PredictiveGestureNavPhase.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                transition
                        ? "Android.PredictiveGestureNavigation.WithTransition"
                        : "Android.PredictiveGestureNavigation.WithoutTransition",
                phase,
                PredictiveGestureNavPhase.NUM_ENTRIES);
    }

    /**
     * Record how long the feed stream is restored on NTP.
     *
     * @param duration The duration of feed restoration.
     */
    public static void recordNTPFeedRestorationDuration(long duration) {
        RecordHistogram.recordTimesHistogram(
                "Android.PredictiveNavigationTransition.NTPFeedRestorationDuration", duration);
    }

    /**
     * Record if NTP smooth transition is triggered by fallback or because of restored feed stream.
     *
     * @param byFallback True if the smooth transition is triggered by fallback.
     */
    public static void recordNTPSmoothTransitionMethod(boolean byFallback) {
        RecordHistogram.recordBooleanHistogram(
                "Android.PredictiveNavigationTransition.NTPSmoothTransitionByFallback", byFallback);
    }

    /**
     * @param betweenNativePages True if this navigation is from a chrome native page to another
     *     native page.
     */
    public static void recordNavigateBetweenChromeNativePages(boolean betweenNativePages) {
        RecordHistogram.recordBooleanHistogram(
                "Android.PredictiveNavigationTransition.NavigateBetweenNativePages",
                betweenNativePages);
    }

    /**
     * @param reason The reason why a fallback ux should be used during capturing a native page.
     */
    public static void recordCaptureNativeViewResult(@CaptureNativeViewResult int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PredictiveNavigationTransition.CaptureNativeViewResult",
                reason,
                CaptureNativeViewResult.NUM_ENTRIES);
    }

    /**
     * The delay used by the fallback of NTP smooth transition in case the restoring state is not
     * correctly supplied.
     *
     * @return The max fallback delay.
     */
    public static long maxFallbackDelayOfNtpSmoothTransition() {
        return (long)
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS,
                        "max_fallback_delay_ntp_smooth_transition",
                        1500);
    }
}
