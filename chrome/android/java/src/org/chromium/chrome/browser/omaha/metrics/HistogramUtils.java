// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Source;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Type;
import org.chromium.chrome.browser.omaha.metrics.UpdateSuccessMetrics.AttributionType;

/** A helper class for creating and saving histograms related to update success. */
class HistogramUtils {
    /**
     * Records a histogram for whether or an update is running at the time an update starts.
     * @param alreadyUpdating Whether or not an update is currently tracked as running.
     */
    public static void recordStartedUpdateHistogram(boolean alreadyUpdating) {
        new CachedMetrics.BooleanHistogramSample("GoogleUpdate.StartingUpdateState")
                .record(alreadyUpdating);
    }

    /**
     * Records success or failure histograms for a current update.
     * @param attribution The attribution (see {@link UpdateSuccessMetrics#AttributionType}).
     * @param info        The {@link Tracking} instance that stores the update specifics.
     * @param success     Whether or not the update was deemed successful.
     */
    public static void recordResultHistogram(
            @AttributionType int attribution, Tracking info, boolean success) {
        new CachedMetrics.BooleanHistogramSample(buildResultHistogram(attribution, null))
                .record(success);
        new CachedMetrics.BooleanHistogramSample(buildResultHistogram(attribution, info))
                .record(success);
    }

    /**
     * Used by {@link #recordResultHistogram(int, Tracking, boolean)}.  Exposed for testing.
     * @param attribution The attribution (see {@link UpdateSuccessMetrics#AttributionType}).
     * @param info        The {@link Tracking} instance that stores the update specifics.
     * @return            The generated histogram name.
     */
    private static String buildResultHistogram(@AttributionType int attribution, Tracking info) {
        String histogram = "GoogleUpdate.Result." + attributionToHistogram(attribution);
        if (info == null) return histogram;

        histogram += "." + typeToHistogram(info.getType());
        histogram += "." + sourceToHistogram(info.getSource());
        return histogram;
    }

    private static String typeToHistogram(Type type) {
        if (type == Type.INTENT) {
            return "Intent";
        } else if (type == Type.INLINE) {
            return "Inline";
        } else {
            return "Unknown";
        }
    }

    private static String sourceToHistogram(Source source) {
        if (source == Source.FROM_MENU) {
            return "Menu";
        } else if (source == Source.FROM_INFOBAR) {
            return "Infobar";
        } else if (source == Source.FROM_NOTIFICATION) {
            return "Notification";
        } else {
            return "Unknown";
        }
    }

    private static String attributionToHistogram(@AttributionType int attribution) {
        switch (attribution) {
            case AttributionType.SESSION:
                return "Session";
            case AttributionType.TIME_WINDOW:
                return "TimeWindow";
            default:
                return "Unknown";
        }
    }
}
