// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper Class for GCM UMA Collection.
 */
public class GcmUma {
    // Values for the "Invalidations.GCMUpstreamRequest" UMA histogram. The list is append-only.
    public static final int UMA_UPSTREAM_SUCCESS = 0;
    public static final int UMA_UPSTREAM_SIZE_LIMIT_EXCEEDED = 1;
    public static final int UMA_UPSTREAM_TOKEN_REQUEST_FAILED = 2;
    public static final int UMA_UPSTREAM_SEND_FAILED = 3;
    public static final int UMA_UPSTREAM_COUNT = 4;

    // Keep in sync with the WebPushDeviceState enum in enums.xml.
    @IntDef({WebPushDeviceState.NOT_IDLE_NOT_HIGH_PRIORITY,
            WebPushDeviceState.NOT_IDLE_HIGH_PRIORITY, WebPushDeviceState.IDLE_NOT_HIGH_PRIORITY,
            WebPushDeviceState.IDLE_HIGH_PRIORITY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface WebPushDeviceState {
        int NOT_IDLE_NOT_HIGH_PRIORITY = 0;
        int NOT_IDLE_HIGH_PRIORITY = 1;
        int IDLE_NOT_HIGH_PRIORITY = 2;
        int IDLE_HIGH_PRIORITY = 3;
        int NUM_ENTRIES = 4;
    }

    public static void recordDataMessageReceived(Context context, final boolean hasCollapseKey) {
        onNativeLaunched(context, new Runnable() {
            @Override public void run() {
                // There is no equivalent of the GCM Store on Android in which we can fail to find a
                // registered app. It's not clear whether Google Play Services doesn't check for
                // registrations, or only gives us messages that have one, but in either case we
                // should log true here.
                RecordHistogram.recordBooleanHistogram(
                        "GCM.DataMessageReceivedHasRegisteredApp", true);
                RecordHistogram.recordCountHistogram(
                        "GCM.DataMessageReceived", 1);
                RecordHistogram.recordBooleanHistogram(
                        "GCM.DataMessageReceivedHasCollapseKey", hasCollapseKey);
            }
        });
    }

    public static void recordGcmUpstreamHistogram(Context context, final int value) {
        onNativeLaunched(context, new Runnable() {
            @Override public void run() {
                RecordHistogram.recordEnumeratedHistogram(
                        "Invalidations.GCMUpstreamRequest", value, UMA_UPSTREAM_COUNT);
            }
        });
    }

    public static void recordDeletedMessages(Context context) {
        onNativeLaunched(context, new Runnable() {
            @Override public void run() {
                RecordHistogram.recordCount1000Histogram(
                        "GCM.DeletedMessagesReceived", 0 /* unknown deleted count */);
            }
        });
    }

    public static void recordSubscriptionLazyCheckTime(long time) {
        // Use {@link CachedMetrics} so this gets reported when native is loaded instead of calling
        // native right away.
        new CachedMetrics.TimesHistogramSample("PushMessaging.TimeToCheckIfSubscriptionLazy")
                .record(time);
    }

    public static void recordWebPushReceivedDeviceState(@WebPushDeviceState int state) {
        // Use {@link CachedMetrics} so this gets reported when native is loaded instead of calling
        // native right away.
        new CachedMetrics
                .EnumeratedHistogramSample(
                        "GCM.WebPushReceived.DeviceState", WebPushDeviceState.NUM_ENTRIES)
                .record(state);
    }

    private static void onNativeLaunched(final Context context, final Runnable task) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .addStartupCompletedObserver(new StartupCallback() {
                            @Override
                            public void onSuccess() {
                                task.run();
                            }

                            @Override
                            public void onFailure() {
                                // Startup failed.
                            }
                        });
            }
        });
    }
}
