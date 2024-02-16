// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics util class. */
class GestureNavMetrics {
    // Used to record the UMA histogram GestureNavigation. This definition should be
    // in sync with the enum "GestureNavigationDirection" in tools/metrics/histograms/enums.xml.
    @IntDef({GestureNavigationDirection.BACK, GestureNavigationDirection.FORWARD})
    @Retention(RetentionPolicy.SOURCE)
    private @interface GestureNavigationDirection {
        int BACK = 0;
        int FORWARD = 1;

        int NUM_ENTRIES = 2;
    }

    // Should be in sync with the enum "GestureNavigationType" in
    // tools/metrics/histograms/enums.xml.
    @SuppressWarnings("unused")
    @IntDef({GestureNavigationType.CHROME, GestureNavigationType.SYSTEM})
    @Retention(RetentionPolicy.SOURCE)
    private @interface GestureNavigationType {
        int SYSTEM = 0;
        int CHROME = 1;

        int NUM_ENTRIES = 2;
    }

    private GestureNavMetrics() {}

    /**
     * Records UMA histogram for various gesture navigation events.
     *
     * @param name Event name.
     * @param forward {@code true} if navigating forward; otherwise {@code false}.
     */
    static void recordHistogram(String name, boolean forward) {
        RecordHistogram.recordEnumeratedHistogram(
                name,
                forward ? GestureNavigationDirection.FORWARD : GestureNavigationDirection.BACK,
                GestureNavigationDirection.NUM_ENTRIES);
    }

    /**
     * Records UMA user action for navigation popup events.
     * @param name Event name.
     */
    static void recordUserAction(String name) {
        RecordUserAction.record("BackMenu_" + name);
    }

    /**
     * Records UMA histogram that tells which gesture navigation type is being used.
     * @param isChromeGesture {@code true} if Chrome's own UI is in use.
     */
    static void logGestureType(boolean isChromeGesture) {
        // true  -> GestureNavigationType.CHROME
        // false -> GestureNavigationType.SYSTEM
        // This histogram is logged at Chrome startup.
        RecordHistogram.recordBooleanHistogram("GestureNavigation.Type2", isChromeGesture);
    }
}
