// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class to contain metrics recording constants and behaviour for Browser Services.
 */
public class BrowserServicesMetrics {
    @IntDef({VerificationResult.ONLINE_SUCCESS, VerificationResult.ONLINE_FAILURE,
            VerificationResult.OFFLINE_SUCCESS, VerificationResult.OFFLINE_FAILURE,
            VerificationResult.HTTPS_FAILURE, VerificationResult.REQUEST_FAILURE,
            VerificationResult.CACHED_SUCCESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface VerificationResult {
        // Don't reuse values or reorder values. If you add something new, change NUM_ENTRIES as
        // well.
        int ONLINE_SUCCESS = 0;
        int ONLINE_FAILURE = 1;
        int OFFLINE_SUCCESS = 2;
        int OFFLINE_FAILURE = 3;
        int HTTPS_FAILURE = 4;
        int REQUEST_FAILURE = 5;
        int CACHED_SUCCESS = 6;
        int NUM_ENTRIES = 7;
    }

    /**
     * Records the verification result for Trusted Web Activity verification.
     */
    public static void recordVerificationResult(@VerificationResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "BrowserServices.VerificationResult", result, VerificationResult.NUM_ENTRIES);
    }

    public static void recordVerificationTime(long duration, boolean online) {
        RecordHistogram.recordTimesHistogram(
                online ? "BrowserServices.VerificationTime.Online"
                        : "BrowserServices.VerificationTime.Offline", duration);
    }

    /**
     * Returns a {@link TimingMetric} that records the amount of time spent querying the Android
     * system for ResolveInfos that will deal with a given URL when launching from a background
     * service.
     */
    public static TimingMetric getServiceTabResolveInfoTimingContext() {
        return new TimingMetric("BrowserServices.ServiceTabResolveInfoQuery");
    }

    /**
     * Returns a {@link TimingMetric} that records the amount of time spent opening the
     * {@link ClientAppDataRegister}.
     */
    public static TimingMetric getClientAppDataLoadTimingContext() {
        return new TimingMetric("BrowserServices.ClientAppDataLoad");
    }

    /**
     * Returns a {@link TimingMetric} that records the amount of time taken to check if a package
     * handles a Browsable intent.
     */
    public static TimingMetric getBrowsableIntentResolutionTimingContext() {
        return new TimingMetric("BrowserServices.BrowsableIntentCheck");
    }

    /**
     * A class to be used with a try-with-resources to record the elapsed time within the try block.
     */
    public static class TimingMetric implements AutoCloseable {
        private final String mMetric;
        private final long mStart;

        private static long now() {
            return SystemClock.uptimeMillis();
        }

        public TimingMetric(String metric) {
            mMetric = metric;
            mStart = now();
        }

        @Override
        public void close() {
            // Use {@link CachedMetrics} so this can be called before native is loaded.
            new CachedMetrics.MediumTimesHistogramSample(mMetric).record(now() - mStart);
        }
    }

    // Don't let anyone instantiate.
    private BrowserServicesMetrics() {}
}
