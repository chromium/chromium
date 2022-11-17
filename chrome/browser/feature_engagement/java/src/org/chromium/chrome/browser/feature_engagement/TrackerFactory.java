// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_engagement;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;

/**
 * This factory creates Tracker for the given {@link Profile}.
 */
public final class TrackerFactory {

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
        return TrackerFactoryJni.get().getTrackerForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        Tracker getTrackerForProfile(Profile profile);
        void setTestingFactory(Profile profile, Tracker testTracker);
    }
}
