// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.scheduler;

import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;

/** Used by tips notifications to schedule and display tips through the Android UI. */
@NullMarked
public class TipsAgent {
    @CalledByNative
    private static void showTipsPromo(@TipsNotificationsFeatureType int featureType) {
        Context context = ContextUtils.getApplicationContext();
        Intent newIntent = IntentHandler.createTrustedOpenNewTabIntent(context, false);
        newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        newIntent.putExtra(IntentHandler.EXTRA_TIPS_NOTIFICATION_FEATURE_TYPE, featureType);
        IntentHandler.setTabLaunchType(newIntent, TabLaunchType.FROM_TIPS_NOTIFICATIONS);
        context.startActivity(newIntent);
    }

    /**
     * Maybe schedule a tips notification depending on backend criteria.
     *
     * @param profile The current profile.
     * @param isBottomOmnibox Whether the omnibox is in the bottom position or not.
     */
    public static void maybeScheduleNotification(Profile profile, boolean isBottomOmnibox) {
        TipsAgentJni.get().maybeScheduleNotification(profile, isBottomOmnibox);
    }

    /**
     * Remove all pending tips notifications.
     *
     * @param profile The current profile.
     */
    public static void removePendingNotifications(Profile profile) {
        TipsAgentJni.get().removePendingNotifications(profile);
    }

    private TipsAgent() {}

    @NativeMethods
    interface Natives {
        void maybeScheduleNotification(
                @JniType("Profile*") Profile profile, boolean isBottomOmnibox);

        void removePendingNotifications(@JniType("Profile*") Profile profile);
    }
}
