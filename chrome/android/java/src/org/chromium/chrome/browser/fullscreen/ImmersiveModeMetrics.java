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
 *   <li>Exit Reason Breakdown: {@code Android.ImmersiveMode.ExitReason}
 *   <li>Session Duration: {@link #HISTOGRAM_SESSION_DURATION}
 *   <li>Exit Snackbar Shown vs. Interaction: {@code Android.ImmersiveMode.ExitSnackbarShown} &
 *       {@code ExitFullscreen_PressAndHoldEsc}
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

    /** Session Duration: Duration (in ms) of each Browser immersive mode session. */
    public static final String HISTOGRAM_SESSION_DURATION = "Android.ImmersiveMode.SessionDuration";

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
     * Records per-session metrics when exiting a Browser immersive mode session:
     *
     * <ul>
     *   <li>Navigations per Session: {@link #HISTOGRAM_NAVIGATIONS_PER_SESSION}
     *   <li>Tab Switches per Session: {@link #HISTOGRAM_TAB_SWITCHES_PER_SESSION}
     *   <li>Session Duration: {@link #HISTOGRAM_SESSION_DURATION}
     * </ul>
     *
     * @param navigationsCount Number of cross-document navigations during the session.
     * @param tabSwitchesCount Number of tab switches during the session.
     * @param durationMs Total duration of the session in milliseconds.
     */
    public static void recordSessionMetrics(
            int navigationsCount, int tabSwitchesCount, long durationMs) {
        RecordHistogram.recordCount100Histogram(
                HISTOGRAM_NAVIGATIONS_PER_SESSION, navigationsCount);
        RecordHistogram.recordCount100Histogram(
                HISTOGRAM_TAB_SWITCHES_PER_SESSION, tabSwitchesCount);
        RecordHistogram.recordLongTimesHistogram100(HISTOGRAM_SESSION_DURATION, durationMs);
    }
}
