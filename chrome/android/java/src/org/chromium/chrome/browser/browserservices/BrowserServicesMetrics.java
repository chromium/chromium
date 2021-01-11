// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browserservices.verification.OriginVerifier;

/**
 * Class to contain metrics recording constants and behaviour for Browser Services.
 */
public class BrowserServicesMetrics {
    /** Implementation of {@link OriginVerifier.MetricsListener}. */
    public static class OriginVerifierMetricsListener implements OriginVerifier.MetricsListener {
        @Override
        public void recordVerificationResult(@OriginVerifier.VerificationResult int result) {
            RecordHistogram.recordEnumeratedHistogram("BrowserServices.VerificationResult", result,
                    OriginVerifier.VerificationResult.NUM_ENTRIES);
        }

        @Override
        public void recordVerificationTime(long duration, boolean online) {
            if (online) {
                RecordHistogram.recordTimesHistogram(
                        "BrowserServices.VerificationTime.Online", duration);
            } else {
                RecordHistogram.recordTimesHistogram(
                        "BrowserServices.VerificationTime.Offline", duration);
            }
        }
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

        private TimingMetric(String metric) {
            mMetric = metric;
            mStart = now();
        }

        @Override
        public void close() {
            RecordHistogram.recordMediumTimesHistogram(mMetric, now() - mStart);
        }
    }

    // Don't let anyone instantiate.
    private BrowserServicesMetrics() {}
}
