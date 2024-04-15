// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.Bundle;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Bridge for native |AndroidSessionDurationsService| state, for storage and retrieval
 * of Incognito session duration metrics.
 */
public class AndroidSessionDurationsServiceState {
    private static final String INCOGNITO_SESSION_STARTUP_TIME = "incognito_session_startup_time";
    private static final String INCOGNITO_SESSION_LAST_REPORTED_DURATION =
            "incognito_session_last_reported_duration";

    // Session start time, converted from native base::Time.
    private final long mSessionStartTime;
    // Last reported duration in minutes.
    private final long mLastReportedDuration;

    @CalledByNative
    private AndroidSessionDurationsServiceState(long sessionStartTime, long lastReportedDuration) {
        mLastReportedDuration = lastReportedDuration;
        mSessionStartTime = sessionStartTime;
    }

    @CalledByNative
    private long getLastReportedDuration() {
        return mLastReportedDuration;
    }

    @CalledByNative
    private long getSessionStartTime() {
        return mSessionStartTime;
    }

    /**
     * Restores the Android session duration service on Native from serialized data.
     * This function does not supported regular profiles.
     *
     * @param Bundle inState, saved Incognito session duration service state.
     * @param profile Profile, the Incognito profile for which the duration
     *   service will be restored.
     *
     */
    public static void restoreNativeFromSerialized(Bundle inState, Profile profile) {
        long sessionStartTime = inState.getLong(INCOGNITO_SESSION_STARTUP_TIME, -1);
        if (sessionStartTime == -1) {
            return;
        }

        long lastReportedDuration = inState.getLong(INCOGNITO_SESSION_LAST_REPORTED_DURATION, -1);
        assert lastReportedDuration != -1;

        AndroidSessionDurationsServiceStateJni.get()
                .restoreAndroidSessionDurationsServiceState(
                        profile,
                        new AndroidSessionDurationsServiceState(
                                sessionStartTime, lastReportedDuration));
    }

    /**
     * Serializes the data from Android session duration service on native.
     * This function is ONLY supported for Incognito profiles.
     *
     * @param Bundle outState, bundle to save Incognito session duration service state.
     * @param profile Profile, the Incognito profile for which the duration
     *   service will be serialized.
     */
    public static void serializeFromNative(Bundle outState, Profile profile) {
        AndroidSessionDurationsServiceState data =
                AndroidSessionDurationsServiceStateJni.get()
                        .getAndroidSessionDurationsServiceState(profile);
        outState.putLong(INCOGNITO_SESSION_STARTUP_TIME, data.getSessionStartTime());
        outState.putLong(INCOGNITO_SESSION_LAST_REPORTED_DURATION, data.getLastReportedDuration());
    }

    @NativeMethods
    public interface Natives {
        AndroidSessionDurationsServiceState getAndroidSessionDurationsServiceState(
                @JniType("Profile*") Profile profile);

        void restoreAndroidSessionDurationsServiceState(
                @JniType("Profile*") Profile profile,
                AndroidSessionDurationsServiceState durationService);
    }
}
