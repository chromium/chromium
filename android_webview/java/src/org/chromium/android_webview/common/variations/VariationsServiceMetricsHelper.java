// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.variations;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

/**
 * Stores values related to the collection of variations service metrics.
 *
 * The values maintained by this class can be initialized from and serialized to a dedicated
 * variations SharedPreferences, or from a Bundle suitable for sending in AIDL IPC calls.
 */
public class VariationsServiceMetricsHelper {
    private static final String PREF_FILE_NAME = "variations_prefs";

    // The time in milliseconds between the start of the two most recent seed downloads.
    private static final String JOB_INTERVAL = "job_interval";
    // The time in milliseconds between scheduling and executing the last seed download job.
    private static final String JOB_QUEUE_TIME = "job_queue_time";
    // The result of the last seed download.

    // Name of a SharedPreferences pref that stores the time in milliseconds since UNIX epoch of
    // when the most recent seed download job was scheduled.
    private static final String LAST_ENQUEUE_TIME = "last_enqueue_time";
    // Name of a SharedPreferences pref that stores the time in milliseconds since UNIX epoch of
    // when the most recent seed download job was started.
    private static final String LAST_JOB_START_TIME = "last_job_start_time";

    /**
     * Creates a new VariationsServiceMetricsHelper instance initialized with the contents of the
     * given Bundle.
     */
    public static VariationsServiceMetricsHelper fromBundle(Bundle bundle) {
        return new VariationsServiceMetricsHelper(bundle);
    }

    /**
     * Creates a new VariationsServiceMetricsHelper instance initialized with the contents of the
     * variations SharedPreferences instance for the given Context.
     */
    public static VariationsServiceMetricsHelper fromVariationsSharedPreferences(Context context) {
        Bundle bundle = new Bundle();
        SharedPreferences prefs =
                context.getSharedPreferences(PREF_FILE_NAME, Context.MODE_PRIVATE);
        if (prefs.contains(JOB_INTERVAL)) {
            bundle.putLong(JOB_INTERVAL, prefs.getLong(JOB_INTERVAL, 0));
        }
        if (prefs.contains(JOB_QUEUE_TIME)) {
            bundle.putLong(JOB_QUEUE_TIME, prefs.getLong(JOB_QUEUE_TIME, 0));
        }
        if (prefs.contains(LAST_ENQUEUE_TIME)) {
            bundle.putLong(LAST_ENQUEUE_TIME, prefs.getLong(LAST_ENQUEUE_TIME, 0));
        }
        if (prefs.contains(LAST_JOB_START_TIME)) {
            bundle.putLong(LAST_JOB_START_TIME, prefs.getLong(LAST_JOB_START_TIME, 0));
        }
        return new VariationsServiceMetricsHelper(bundle);
    }

    private final Bundle mBundle;

    public Bundle toBundle() {
        // Create a copy of the Bundle to make sure it won't be modified by future mutator method
        // calls in this class.
        return new Bundle(mBundle);
    }

    // This method should only be called from within WebView's service.
    public boolean writeMetricsToVariationsSharedPreferences(Context context) {
        SharedPreferences.Editor prefsEditor =
                context.getSharedPreferences(PREF_FILE_NAME, Context.MODE_PRIVATE).edit();
        prefsEditor.clear();
        if (hasJobInterval()) {
            prefsEditor.putLong(JOB_INTERVAL, getJobInterval());
        }
        if (hasJobQueueTime()) {
            prefsEditor.putLong(JOB_QUEUE_TIME, getJobQueueTime());
        }
        if (hasLastEnqueueTime()) {
            prefsEditor.putLong(LAST_ENQUEUE_TIME, getLastEnqueueTime());
        }
        if (hasLastJobStartTime()) {
            prefsEditor.putLong(LAST_JOB_START_TIME, getLastJobStartTime());
        }
        return prefsEditor.commit();
    }

    public void clearJobInterval() {
        mBundle.remove(JOB_INTERVAL);
    }

    public void setJobInterval(long seedFetchTime) {
        mBundle.putLong(JOB_INTERVAL, seedFetchTime);
    }

    public boolean hasJobInterval() {
        return mBundle.containsKey(JOB_INTERVAL);
    }

    public long getJobInterval() {
        return mBundle.getLong(JOB_INTERVAL);
    }

    public void clearJobQueueTime() {
        mBundle.remove(JOB_QUEUE_TIME);
    }

    public void setJobQueueTime(long seedFetchTime) {
        mBundle.putLong(JOB_QUEUE_TIME, seedFetchTime);
    }

    public boolean hasJobQueueTime() {
        return mBundle.containsKey(JOB_QUEUE_TIME);
    }

    public long getJobQueueTime() {
        return mBundle.getLong(JOB_QUEUE_TIME);
    }

    public void clearLastEnqueueTime() {
        mBundle.remove(LAST_ENQUEUE_TIME);
    }

    public void setLastEnqueueTime(long seedFetchTime) {
        mBundle.putLong(LAST_ENQUEUE_TIME, seedFetchTime);
    }

    public boolean hasLastEnqueueTime() {
        return mBundle.containsKey(LAST_ENQUEUE_TIME);
    }

    public long getLastEnqueueTime() {
        return mBundle.getLong(LAST_ENQUEUE_TIME);
    }

    public void clearLastJobStartTime() {
        mBundle.remove(LAST_JOB_START_TIME);
    }

    public void setLastJobStartTime(long seedFetchTime) {
        mBundle.putLong(LAST_JOB_START_TIME, seedFetchTime);
    }

    public boolean hasLastJobStartTime() {
        return mBundle.containsKey(LAST_JOB_START_TIME);
    }

    public long getLastJobStartTime() {
        return mBundle.getLong(LAST_JOB_START_TIME);
    }

    private VariationsServiceMetricsHelper(Bundle bundle) {
        mBundle = bundle;
    }
}
