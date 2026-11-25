// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to record UMA histograms for Browser immersive mode:
 *
 * <ul>
 *   <li>Immersive Sessions: {@code ToggleFullscreen} UserMetricsAction (recorded in C++ {@code
 *       fullscreen_controller.cc}).
 *   <li>Entry Method Split: {@link #HISTOGRAM_ENTRY_METHOD}
 *   <li>Navigations per Session: {@link #HISTOGRAM_NAVIGATIONS_PER_SESSION}
 *   <li>Tab Switches per Session: {@link #HISTOGRAM_TAB_SWITCHES_PER_SESSION}
 *   <li>Exit Reason Breakdown: {@link #HISTOGRAM_EXIT_REASON}
 *   <li>Session Duration: {@link #HISTOGRAM_SESSION_DURATION}
 *   <li>Exit Snackbar Shown vs. Interaction: {@link #HISTOGRAM_EXIT_SNACKBAR_SHOWN} and {@code
 *       ExitFullscreen_PressAndHoldEsc}
 * </ul>
 */
@NullMarked
public class ImmersiveModeMetrics {
    /** Entry Method Split: 3-dot menu fullscreen button vs. keyboard shortcut (F11). */
    public static final String HISTOGRAM_ENTRY_METHOD = "Android.ImmersiveMode.EntryMethod";

    /**
     * Navigations per Session: Count of cross-document page navigations within a single Browser
     * immersive mode session (excluding page reloads and New Tab Page loads).
     */
    public static final String HISTOGRAM_NAVIGATIONS_PER_SESSION =
            "Android.ImmersiveMode.NavigationsPerSession";

    /**
     * Tab Switches per Session: Count of all tab switches ({@code TabHidingType.CHANGED_TABS})
     * while remaining in a single Browser immersive mode session. Does not differentiate between
     * how the tab switch was triggered (e.g. Ctrl + Tab, Ctrl + T, Ctrl + W, etc.).
     */
    public static final String HISTOGRAM_TAB_SWITCHES_PER_SESSION =
            "Android.ImmersiveMode.TabSwitchesPerSession";

    /**
     * Exit Reason Breakdown: How the user exited Browser immersive mode (back-swipe, long-press
     * Esc, keyboard shortcut, desktop windowing mode change, activity backgrounded/destroyed, or
     * other).
     */
    public static final String HISTOGRAM_EXIT_REASON = "Android.ImmersiveMode.ExitReason";

    /** Session Duration: Duration (in ms) of each Browser immersive mode session. */
    public static final String HISTOGRAM_SESSION_DURATION = "Android.ImmersiveMode.SessionDuration";

    /**
     * Exit Snackbar Shown: Whether the Browser immersive mode exit snackbar was shown on initial
     * entry vs. re-shown during the session (e.g. when holding Esc for more than 0.5s).
     */
    public static final String HISTOGRAM_EXIT_SNACKBAR_SHOWN =
            "Android.ImmersiveMode.ExitSnackbarShown";

    /**
     * Entry method used to enter Browser immersive mode.
     *
     * <p>Entries should not be renumbered and numeric values should never be reused. Keep in sync
     * with ImmersiveModeEntryMethod in tools/metrics/histograms/metadata/android/enums.xml.
     */
    // LINT.IfChange(ImmersiveModeEntryMethod)
    @IntDef({
        ImmersiveModeEntryMethod.APP_MENU_FULLSCREEN_BUTTON,
        ImmersiveModeEntryMethod.KEYBOARD_SHORTCUT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ImmersiveModeEntryMethod {
        int APP_MENU_FULLSCREEN_BUTTON = 0;
        int KEYBOARD_SHORTCUT = 1;
        int NUM_ENTRIES = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:ImmersiveModeEntryMethod)

    /**
     * Reason for exiting Browser immersive mode.
     *
     * <p>Entries should not be renumbered and numeric values should never be reused. Keep in sync
     * with ImmersiveModeExitReason in tools/metrics/histograms/metadata/android/enums.xml.
     */
    // LINT.IfChange(ImmersiveModeExitReason)
    @IntDef({
        ImmersiveModeExitReason.BACK_PRESS,
        ImmersiveModeExitReason.PRESS_AND_HOLD_ESCAPE,
        ImmersiveModeExitReason.KEYBOARD_SHORTCUT,
        ImmersiveModeExitReason.DESKTOP_WINDOWING_MODE_CHANGED,
        ImmersiveModeExitReason.ACTIVITY_HIDDEN_OR_DESTROYED,
        ImmersiveModeExitReason.OTHER
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ImmersiveModeExitReason {
        int BACK_PRESS = 0;
        int PRESS_AND_HOLD_ESCAPE = 1;
        int KEYBOARD_SHORTCUT = 2;
        int DESKTOP_WINDOWING_MODE_CHANGED = 3;
        int ACTIVITY_HIDDEN_OR_DESTROYED = 4;
        int OTHER = 5;
        int NUM_ENTRIES = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:ImmersiveModeExitReason)

    /**
     * Context in which the Browser immersive mode exit snackbar is shown.
     *
     * <p>Entries should not be renumbered and numeric values should never be reused. Keep in sync
     * with ImmersiveModeExitSnackbarShown in tools/metrics/histograms/metadata/android/enums.xml.
     */
    // LINT.IfChange(ImmersiveModeExitSnackbarShown)
    @IntDef({
        ImmersiveModeExitSnackbarShown.INITIAL_ENTRY,
        ImmersiveModeExitSnackbarShown.RETRIGGERED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ImmersiveModeExitSnackbarShown {
        int INITIAL_ENTRY = 0;
        int RETRIGGERED = 1;
        int NUM_ENTRIES = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:ImmersiveModeExitSnackbarShown)

    /**
     * Entry Method Split: Records how the user entered Browser immersive mode (3-dot menu
     * fullscreen button vs. keyboard shortcut).
     *
     * @param entryMethod The {@link ImmersiveModeEntryMethod} used to enter Browser immersive mode.
     */
    public static void recordEntryMethod(@ImmersiveModeEntryMethod int entryMethod) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_ENTRY_METHOD, entryMethod, ImmersiveModeEntryMethod.NUM_ENTRIES);
    }

    /**
     * Exit Reason Breakdown: Records how the user exited Browser immersive mode.
     *
     * @param exitReason The {@link ImmersiveModeExitReason} that caused Browser immersive mode to
     *     exit.
     */
    public static void recordExitReason(@ImmersiveModeExitReason int exitReason) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_EXIT_REASON, exitReason, ImmersiveModeExitReason.NUM_ENTRIES);
    }

    /**
     * Exit Snackbar Shown: Records when the exit snackbar is shown in Browser immersive mode (on
     * initial entry vs. re-shown when holding Esc for more than 0.5s).
     *
     * @param event The {@link ImmersiveModeExitSnackbarShown} event.
     */
    public static void recordExitSnackbarShown(@ImmersiveModeExitSnackbarShown int event) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_EXIT_SNACKBAR_SHOWN, event, ImmersiveModeExitSnackbarShown.NUM_ENTRIES);
    }

    /**
     * Records per-session metrics when exiting a Browser immersive mode session:
     *
     * <ul>
     *   <li>Navigations per Session: {@link #HISTOGRAM_NAVIGATIONS_PER_SESSION}
     *   <li>Tab Switches per Session: {@link #HISTOGRAM_TAB_SWITCHES_PER_SESSION}
     *   <li>Exit Reason Breakdown: {@link #HISTOGRAM_EXIT_REASON}
     *   <li>Session Duration: {@link #HISTOGRAM_SESSION_DURATION}
     * </ul>
     *
     * @param navigationsCount Number of cross-document navigations during the session.
     * @param tabSwitchesCount Number of tab switches during the session.
     * @param durationMs Total duration of the session in milliseconds.
     * @param exitReason The {@link ImmersiveModeExitReason} that ended the session.
     */
    public static void recordSessionMetrics(
            int navigationsCount,
            int tabSwitchesCount,
            long durationMs,
            @ImmersiveModeExitReason int exitReason) {
        RecordHistogram.recordCount100Histogram(
                HISTOGRAM_NAVIGATIONS_PER_SESSION, navigationsCount);
        RecordHistogram.recordCount100Histogram(
                HISTOGRAM_TAB_SWITCHES_PER_SESSION, tabSwitchesCount);
        RecordHistogram.recordLongTimesHistogram100(HISTOGRAM_SESSION_DURATION, durationMs);
        recordExitReason(exitReason);
    }
}
