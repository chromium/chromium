// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Map;

/** Bridge to the native tips notification scheduler (TipsAgentAndroid). */
@NullMarked
final class TipsSchedulerBridge {
    /**
     * Maybe schedule a tips notification depending on backend criteria. If a notification is
     * already scheduled, this will reschedule it.
     *
     * @param profile The current profile.
     * @param customSignals Java-only runtime signals, keyed by signal name, merged with natively
     *     retrieved signals when determining tip eligibility.
     */
    static void maybeScheduleNotification(Profile profile, Map<String, Float> customSignals) {
        TipsSchedulerBridgeJni.get().maybeScheduleNotification(profile, customSignals);
    }

    /**
     * Returns whether scheduling a tips notification is still worthwhile, i.e. false once every tip
     * has been shown (self-service only; always true on the legacy path).
     *
     * @param profile The current profile.
     */
    static boolean hasRemainingTips(Profile profile) {
        return TipsSchedulerBridgeJni.get().hasRemainingTips(profile);
    }

    /**
     * Remove all pending tips notifications.
     *
     * @param profile The current profile.
     */
    static void removePendingNotifications(Profile profile) {
        TipsSchedulerBridgeJni.get().removePendingNotifications(profile);
    }

    private TipsSchedulerBridge() {}

    @NativeMethods
    interface Natives {
        void maybeScheduleNotification(
                @JniType("Profile*") Profile profile,
                @JniType("std::map<std::string, float>") Map<String, Float> customSignals);

        boolean hasRemainingTips(@JniType("Profile*") Profile profile);

        void removePendingNotifications(@JniType("Profile*") Profile profile);
    }
}
