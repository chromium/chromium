// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.EntryPoint;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.EntryPointTabType;
import org.chromium.dom_distiller.mojom.FontFamily;
import org.chromium.dom_distiller.mojom.Theme;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics utils for use in reader mode. */
@NullMarked
public class ReaderModeMetrics {
    @VisibleForTesting
    public static final String READER_MODE_CONTEXTUAL_PAGE_ACTION_EVENT_HISTOGRAM =
            "DomDistiller.Android.ReaderModeContextualPageActionEvent";

    // LINT.IfChange(ReaderModeContextualPageActionEvent)

    /**
     * These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused.
     */
    @IntDef({
        ReaderModeContextualPageActionEvent.UNKNOWN,
        ReaderModeContextualPageActionEvent.NOT_ELIGIBLE,
        ReaderModeContextualPageActionEvent.ELIGIBLE,
        ReaderModeContextualPageActionEvent.SUPPRESSED,
        ReaderModeContextualPageActionEvent.SHOWN,
        ReaderModeContextualPageActionEvent.TIME_OUT,
        ReaderModeContextualPageActionEvent.CLICKED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ReaderModeContextualPageActionEvent {
        int UNKNOWN = 0;
        int NOT_ELIGIBLE = 1;
        int ELIGIBLE = 2;
        int SUPPRESSED = 3;
        int SHOWN = 4;
        int TIME_OUT = 5;
        int CLICKED = 6;
        int MAX_VALUE = CLICKED;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:ReaderModeContextualPageActionEvent)

    /**
     * Records an action taken by the user on the reader mode contextual page action.
     *
     * @param action The {@link ReaderModeAction} to record.
     */
    public static void recordReaderModeContextualPageActionEvent(
            @ReaderModeContextualPageActionEvent int action) {
        RecordHistogram.recordEnumeratedHistogram(
                READER_MODE_CONTEXTUAL_PAGE_ACTION_EVENT_HISTOGRAM,
                action,
                ReaderModeContextualPageActionEvent.MAX_VALUE);
    }

    /**
     * Record if any distillation signal was in time for the CPA timeout.
     *
     * @param signalAvailable Boolean of whether any distillation signal was available.
     */
    public static void recordAnyPageSignalWithinTimeout(boolean signalAvailable) {
        RecordHistogram.recordBooleanHistogram(
                "DomDistiller.Android.AnyPageSignalWithinTimeout", signalAvailable);
    }

    /**
     * Record whether a positive distillation signal was in time for the CPA timeout.
     *
     * @param signalAvailable Boolean of whether a positive distillation signal was available.
     */
    public static void recordDistillablePageSignalWithinTimeout(boolean signalAvailable) {
        RecordHistogram.recordBooleanHistogram(
                "DomDistiller.Android.DistillablePageSignalWithinTimeout", signalAvailable);
    }

    /**
     * Record how long it takes to get a reader mode result.
     *
     * @param timeMs The time in milliseconds to get a reader mode result.
     */
    public static void recordTimeToProvideResultToAccumulator(long timeMs) {
        RecordHistogram.recordLongTimesHistogram(
                "DomDistiller.Time.TimeToProvideResultToAccumulator", timeMs);
    }

    /**
     * Record the entry-point when reader mode is activated.
     *
     * @param entryPoint The {@link EntryPoint} from which reader mode was activated.
     * @param entryPointTabType The {@link EntryPointTabType} of the tab when reader mode was
     *     activated.
     */
    public static void recordReaderModeEntryPoint(
            @EntryPoint int entryPoint, @EntryPointTabType int entryPointTabType) {
        String tabTypeString = "";
        switch (entryPointTabType) {
            case ReaderModeManager.EntryPointTabType.REGULAR_TAB:
                tabTypeString = "Regular";
                break;
            case ReaderModeManager.EntryPointTabType.CUSTOM_TAB:
                tabTypeString = "CCT";
                break;
            case ReaderModeManager.EntryPointTabType.INCOGNITO_TAB:
                tabTypeString = "Incognito";
                break;
            case ReaderModeManager.EntryPointTabType.INCOGNITO_CUSTOM_TAB:
                tabTypeString = "IncognitoCCT";
                break;
            default:
                assert false : "Invalid entry point tab type provided: " + entryPointTabType;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "DomDistiller.Android.EntryPoint." + tabTypeString,
                entryPoint,
                EntryPoint.MAX_VALUE);
    }

    /** Record when a user enters Reading Mode. */
    public static void recordOnStartedReaderMode() {
        RecordUserAction.record("DomDistiller.Android.OnStartedReaderMode");
    }

    /** Record when a user exits Reading Mode. */
    public static void recordOnStoppedReaderMode() {
        RecordUserAction.record("DomDistiller.Android.OnStoppedReaderMode");
    }

    /**
     * Record the amount of time the user spent in Reader Mode.
     *
     * @param timeMs The amount of time in milliseconds that the user spent in Reader Mode.
     */
    public static void recordReaderModeViewDuration(long timeMs) {
        RecordHistogram.recordLongTimesHistogram("DomDistiller.Time.ViewingReaderModePage", timeMs);
    }

    /** Report when the distilled page prefs are opened. */
    public static void reportReaderModePrefsOpened() {
        RecordUserAction.record("DomDistiller.Android.DistilledPagePrefsOpened");
    }

    /**
     * Report the font family option selected in the prefs.
     *
     * @param fontFamily The selected font family, as a {@link FontFamily.EnumType} integer.
     *     constant.
     */
    public static void reportReaderModePrefsFontFamilyChanged(@FontFamily.EnumType int fontFamily) {
        RecordUserAction.record("DomDistiller.Android.FontFamilyChanged");
        RecordHistogram.recordCount100Histogram(
                "DomDistiller.Android.FontFamilySelected", fontFamily);
    }

    /**
     * Report the font scaling value selected in the prefs.
     *
     * @param value The selected font scaling multiplier, from 1f to 2.5f in 0.25f increments.
     */
    public static void reportReaderModePrefsFontScalingChanged(float value) {
        RecordUserAction.record("DomDistiller.Android.FontScalingChanged");
        // Convert to a percentage for recording purposes.
        // Percentages will range from 100% to 250% in 25% increments.
        int percentage = (int) (value * 100);
        RecordHistogram.recordCount1000Histogram(
                "DomDistiller.Android.FontScalingSelected", percentage);
    }

    /**
     * Report the theme option selected in the prefs.
     *
     * @param theme The selected theme, as a {@link Theme.EnumType} integer constant.
     */
    public static void reportReaderModePrefsThemeChanged(@Theme.EnumType int theme) {
        RecordUserAction.record("DomDistiller.Android.ThemeChanged");
        RecordHistogram.recordCount100Histogram("DomDistiller.Android.ThemeSelected", theme);
    }
}
