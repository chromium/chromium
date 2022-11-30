// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.atomic.AtomicBoolean;

// A helper class to record some stats about nonembedded WebView services.
/* package */ class ServicesStatsHelper {
    private static AtomicBoolean sIsFirstServiceInProcess = new AtomicBoolean(true);

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({NonembeddedService.AW_MINIDUMP_UPLOAD_JOB_SERVICE,
            NonembeddedService.AW_VARIATIONS_SEED_FETCHER,
            NonembeddedService.COMPONENTS_PROVIDER_SERVICE,
            NonembeddedService.CRASH_RECEIVER_SERVICE, NonembeddedService.DEVELOPER_UI_SERVICE,
            NonembeddedService.METRICS_BRIDGE_SERVICE, NonembeddedService.METRICS_UPLOAD_SERVICE,
            NonembeddedService.SAFE_MODE_SERVICE, NonembeddedService.VARIATIONS_SEED_SERVER,
            NonembeddedService.COUNT})
    public @interface NonembeddedService {
        // These values are persisted to logs. Entries should not be renumbered and
        // numeric values should never be reused.
        int AW_MINIDUMP_UPLOAD_JOB_SERVICE = 0;
        int AW_VARIATIONS_SEED_FETCHER = 1;
        int COMPONENTS_PROVIDER_SERVICE = 2;
        int CRASH_RECEIVER_SERVICE = 3;
        int DEVELOPER_UI_SERVICE = 4;
        int METRICS_BRIDGE_SERVICE = 5;
        int METRICS_UPLOAD_SERVICE = 6;
        int SAFE_MODE_SERVICE = 7;
        int VARIATIONS_SEED_SERVER = 8;
        int COUNT = 9;
    }

    static void recordServiceLaunch(@NonembeddedService int service) {
        boolean isFirstServiceInProcess = sIsFirstServiceInProcess.getAndSet(false);

        RecordHistogram.recordBooleanHistogram(
                "Android.WebView.Nonembedded.IsFreshServiceProcessLaunched",
                isFirstServiceInProcess);
        if (isFirstServiceInProcess) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.WebView.Nonembedded.FirstServiceInProcess", service,
                    NonembeddedService.COUNT);
        }
    }
}
