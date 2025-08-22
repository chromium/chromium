// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;

/** Metrics utils for use in reader mode. */
@NullMarked
public class ReaderModeMetrics {
    // TODO(crbug.com/434997280): Add remainder of non-native dom_distiller metric calls here.

    /** Report when the distilled page prefs are opened. */
    public static void reportReaderModePrefsOpened() {
        RecordUserAction.record("DomDistiller.Android.DistilledPagePrefsOpened");
    }

    /** Report the font family option selected in the prefs. */
    public static void reportReaderModePrefsFontFamilyChanged(int fontFamily) {
        RecordUserAction.record("DomDistiller.Android.FontFamilyChanged");
        RecordHistogram.recordCount100Histogram(
                "DomDistiller.Android.FontFamilySelected", fontFamily);
    }

    /** Report the font scaling value selected in the prefs. */
    public static void reportReaderModePrefsFontScalingChanged(float value) {
        RecordUserAction.record("DomDistiller.Android.FontScalingChanged");
        // Convert to a percentage for recording purposes.
        // Percentages will range from 100% to 250% in 25% increments.
        int percentage = (int) (value * 100);
        RecordHistogram.recordCount1000Histogram(
                "DomDistiller.Android.FontScalingSelected", percentage);
    }

    /** Report the theme option selected in the prefs. */
    public static void reportReaderModePrefsThemeChanged(int theme) {
        RecordUserAction.record("DomDistiller.Android.ThemeChanged");
        RecordHistogram.recordCount100Histogram("DomDistiller.Android.ThemeSelected", theme);
    }
}
