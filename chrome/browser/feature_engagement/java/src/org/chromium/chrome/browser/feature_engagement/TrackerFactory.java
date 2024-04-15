// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_engagement;

import androidx.annotation.Nullable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;

/** This factory creates Tracker for the given {@link Profile}. */
public final class TrackerFactory {
    private static Tracker sTrackerForTesting;

    // Don't instantiate me.
    private TrackerFactory() {}

    /**
     * A factory method to build a {@link Tracker} object. Each Profile only ever
     * has a single {@link Tracker}, so the first this method is called (or from
     * native), the {@link Tracker} will be created, and later calls will return
     * the already created instance.
     * @return The {@link Tracker} for the given profile object.
     */
    public static Tracker getTrackerForProfile(Profile profile) {
        if (sTrackerForTesting != null) return sTrackerForTesting;
        if (profile == null) {
            throw new IllegalArgumentException("Profile is required for retrieving tracker.");
        }
        profile.ensureNativeInitialized();
        return TrackerFactoryJni.get().getTrackerForProfile(profile);
    }

    /**
     * Sets up a testing factory in C++ and pass it a Tracker object for wrapping and proxying of
     * calls back up to Java.
     * @param The {@link profile} the current profile object.
     * @param The {@link Tracker} the test tracker for C++ to wrap.
     */
    public static void setTestingFactory(Profile profile, Tracker testTracker) {
        TrackerFactoryJni.get().setTestingFactory(profile, testTracker);
    }

    /**
     * Set a {@Tracker} to use for testing. All calls to {@link #getTrackerForProfile( Profile )}
     * will return the test tracker rather than the real tracker.
     *
     * @param testTracker The {@Tracker} to use for testing, or null if the real tracker should be
     *                    used.
     */
    public static void setTrackerForTests(@Nullable Tracker testTracker) {
        sTrackerForTesting = testTracker;
        ResettersForTesting.register(() -> sTrackerForTesting = null);
    }

    @NativeMethods
    interface Natives {
        Tracker getTrackerForProfile(@JniType("Profile*") Profile profile);

        void setTestingFactory(@JniType("Profile*") Profile profile, Tracker testTracker);
    }
}
