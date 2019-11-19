// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory;

import android.content.ComponentCallbacks2;
import android.content.res.Configuration;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Centralizes UMA data collection for Android-specific memory conditions.
 */
public class MemoryPressureUma implements ComponentCallbacks2 {
    @IntDef({Notification.UNKNOWN_TRIM_LEVEL, Notification.TRIM_MEMORY_COMPLETE,
            Notification.TRIM_MEMORY_MODERATE, Notification.TRIM_MEMORY_BACKGROUND,
            Notification.TRIM_MEMORY_UI_HIDDEN, Notification.TRIM_MEMORY_RUNNING_CRITICAL,
            Notification.TRIM_MEMORY_RUNNING_LOW, Notification.TRIM_MEMORY_RUNNING_MODERATE,
            Notification.ON_LOW_MEMORY})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Notification {
        // WARNING: These values are persisted to logs. Entries should not be
        //          renumbered and numeric values should never be reused.
        // Keep in sync with "Android.MemoryPressureNotification" UMA enum.
        int UNKNOWN_TRIM_LEVEL = 0;
        int TRIM_MEMORY_COMPLETE = 1;
        int TRIM_MEMORY_MODERATE = 2;
        int TRIM_MEMORY_BACKGROUND = 3;
        int TRIM_MEMORY_UI_HIDDEN = 4;
        int TRIM_MEMORY_RUNNING_CRITICAL = 5;
        int TRIM_MEMORY_RUNNING_LOW = 6;
        int TRIM_MEMORY_RUNNING_MODERATE = 7;
        int ON_LOW_MEMORY = 8;

        // Must be the last one.
        int NUM_ENTRIES = 9;
    }

    private final String mHistogramName;

    private static MemoryPressureUma sInstance;

    public static void initializeForBrowser() {
        initializeInstance("Browser");
    }

    public static void initializeForChildService() {
        initializeInstance("ChildService");
    }

    private static void initializeInstance(String processType) {
        ThreadUtils.assertOnUiThread();
        assert sInstance == null;
        sInstance = new MemoryPressureUma(processType);
        ContextUtils.getApplicationContext().registerComponentCallbacks(sInstance);
    }

    private MemoryPressureUma(String processType) {
        mHistogramName = "Android.MemoryPressureNotification." + processType;
    }

    @Override
    public void onLowMemory() {
        record(Notification.ON_LOW_MEMORY);
    }

    @Override
    public void onTrimMemory(int level) {
        switch (level) {
            case TRIM_MEMORY_COMPLETE:
                record(Notification.TRIM_MEMORY_COMPLETE);
                break;
            case TRIM_MEMORY_MODERATE:
                record(Notification.TRIM_MEMORY_MODERATE);
                break;
            case TRIM_MEMORY_BACKGROUND:
                record(Notification.TRIM_MEMORY_BACKGROUND);
                break;
            case TRIM_MEMORY_UI_HIDDEN:
                record(Notification.TRIM_MEMORY_UI_HIDDEN);
                break;
            case TRIM_MEMORY_RUNNING_CRITICAL:
                record(Notification.TRIM_MEMORY_RUNNING_CRITICAL);
                break;
            case TRIM_MEMORY_RUNNING_LOW:
                record(Notification.TRIM_MEMORY_RUNNING_LOW);
                break;
            case TRIM_MEMORY_RUNNING_MODERATE:
                record(Notification.TRIM_MEMORY_RUNNING_MODERATE);
                break;
            default:
                record(Notification.UNKNOWN_TRIM_LEVEL);
                break;
        }
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {}

    private void record(@Notification int notification) {
        RecordHistogram.recordEnumeratedHistogram(
                mHistogramName, notification, Notification.NUM_ENTRIES);
    }
}
