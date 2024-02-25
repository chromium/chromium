// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

/** A ServiceConnection that records a histogram for service connection delay. */
public abstract class ServiceConnectionDelayRecorder implements ServiceConnection {
    private static final String SERVICE_CONNECTION_DELAY_HISTOGRAM_PREFIX =
            "Android.WebView.Startup.NonblockingServiceConnectionDelay.";

    private static final Clock CLOCK = SystemClock::uptimeMillis;

    private long mBindTime = -1;
    private boolean mRecorded;

    /**
     * A mockable clock. Returns milliseconds since boot, not counting time spent in deep sleep. For
     * reference, the default implementation is {@code SystemClock.uptimeMillis()}.
     */
    @VisibleForTesting
    public interface Clock {
        long uptimeMillis();
    }

    /** Bind to the given service. See {@link ServiceHelper#bindService} for details. */
    public final boolean bind(Context context, Intent intent, int flags) {
        mBindTime = getClock().uptimeMillis();
        return ServiceHelper.bindService(context, intent, this, flags);
    }

    @Override
    public final void onServiceConnected(ComponentName className, IBinder service) {
        assert mBindTime != -1 : "Should call bindService first";
        // Only record the first connection.
        if (!mRecorded) {
            long connectionTime = getClock().uptimeMillis();

            String serviceName = className.getShortClassName();
            serviceName = serviceName.substring(serviceName.lastIndexOf(".") + 1);

            RecordHistogram.recordTimesHistogram(
                    SERVICE_CONNECTION_DELAY_HISTOGRAM_PREFIX + serviceName,
                    connectionTime - mBindTime);
            mRecorded = true;
        }

        onServiceConnectedImpl(className, service);
    }

    /** Overridden by tests. */
    @VisibleForTesting
    public Clock getClock() {
        return CLOCK;
    }

    /**
     * This should contain the actual implementation that would have otherwise gone into {@link
     * onServiceConnected}.
     */
    public abstract void onServiceConnectedImpl(ComponentName className, IBinder service);
}
