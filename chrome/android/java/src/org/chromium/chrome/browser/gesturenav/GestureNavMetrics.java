// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Metrics util class.
 */
class GestureNavMetrics {
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
        RecordHistogram.recordBooleanHistogram("GestureNavigation.Type", isChromeGesture);
    }
}
