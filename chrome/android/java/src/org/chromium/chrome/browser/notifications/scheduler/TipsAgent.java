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
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tips.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.tips.TipsUtils;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.concurrent.TimeUnit;

/** Used by tips notifications to schedule and display tips through the Android UI. */
@NullMarked
public class TipsAgent {
    @CalledByNative
    private static void showTipsPromo(@TipsNotificationsFeatureType int featureType) {
        Context context = ContextUtils.getApplicationContext();
        Intent newIntent = IntentHandler.createTrustedOpenNewTabIntent(context, false);
        newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        newIntent.putExtra(IntentHandler.EXTRA_TIPS_NOTIFICATION_FEATURE_TYPE, featureType);
        // Use an existing NTP if available to show the feature promo on, tied into the associated
        // handling switch case statement in ChromeTabbedActivity.java.
        newIntent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.setTabLaunchType(newIntent, TabLaunchType.FROM_TIPS_NOTIFICATIONS);
        context.startActivity(newIntent);
    }

    /**
     * Maybe schedule a tips notification depending on backend criteria. If a notification is
     * already scheduled, this will reschedule it.
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

    /**
     * With a valid profile, schedule a deferred startup task which potentially schedules a feature
     * tip notification based on backend segmentation ranker criteria. Also checks if app-level and
     * tips notifications are also enabled. On all other workflows, ensure that any previously
     * pending notifications are cancelled on either app startup or flag toggle.
     *
     * @param profileProviderSupplier The supplier for the current {@link ProfileProvider}.
     * @param windowAndroid The current {@link WindowAndroid}.
     */
    public static void performNotificationSchedulerSteps(
            OneshotSupplier<ProfileProvider> profileProviderSupplier, WindowAndroid windowAndroid) {
        profileProviderSupplier.onAvailable(
                (provider) -> {
                    Profile profile = provider.getOriginalProfile();
                    if (profile.shutdownStarted()) return;

                    if (ChromeFeatureList.sAndroidTipsNotifications.isEnabled()
                            && TipsUtils.isSupportedDeviceType()) {
                        if (ChromeFeatureList.sAndroidTipsNotificationsResetFeatureTipShown
                                .getValue()) {
                            TipsUtils.clearFeatureTipShownPrefs(profile);
                        }

                        maybeScheduleTipsNotification(profile, windowAndroid);
                    } else {
                        removePendingNotifications(profile);
                    }
                });
    }

    private static void maybeScheduleTipsNotification(
            Profile profile, WindowAndroid windowAndroid) {
        // This may run from the delayed reschedule task below, by which point the profile may
        // have started shutting down. Avoid passing it across JNI in that case.
        if (profile.shutdownStarted() || windowAndroid.isDestroyed()) return;

        boolean isBottomOmnibox = isBottomOmniboxActive(windowAndroid);

        TipsUtils.areTipsNotificationsEnabled(
                (enabled) -> {
                    // If the notification channel is enabled, check if a notification was actually
                    // scheduled before scheduling a task to run the reschedule logic.
                    if (enabled) {
                        // This function includes rescheduling a notification if one is pending.
                        maybeScheduleNotification(profile, isBottomOmnibox);
                        // Run this current function again in 1 hour since the scheduler will
                        // schedule a notification 4 hours out, so if the user is still active on
                        // Chrome then reschedule it. The remove call earlier in this function will
                        // remove all pending notifications and the new scheduling call acts as a
                        // reschedule. If the app is closed (user offline) with a post delayed task
                        // it will be torn down. Note that it is possible that when the notification
                        // is rescheduled, the usage criteria may have changed such that the user is
                        // no longer eligible to receive a notification.
                        WeakReference<WindowAndroid> windowAndroidRef =
                                new WeakReference<>(windowAndroid);
                        PostTask.postDelayedTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    WindowAndroid window = windowAndroidRef.get();
                                    if (window != null && !window.isDestroyed()) {
                                        maybeScheduleTipsNotification(profile, window);
                                    }
                                },
                                TimeUnit.HOURS.toMillis(1));
                    }
                });
    }

    private static boolean isBottomOmniboxActive(WindowAndroid windowAndroid) {
        // Set the default fallback for controls position to be top.
        @ControlsPosition int controlsPosition = ControlsPosition.TOP;
        @Nullable BrowserControlsManager browserControlsManager =
                BrowserControlsManagerSupplier.getValueOrNullFrom(windowAndroid);
        if (browserControlsManager != null) {
            controlsPosition = browserControlsManager.getControlsPosition();
        }
        return controlsPosition == ControlsPosition.BOTTOM;
    }

    private TipsAgent() {}

    @NativeMethods
    interface Natives {
        void maybeScheduleNotification(
                @JniType("Profile*") Profile profile, boolean isBottomOmnibox);

        void removePendingNotifications(@JniType("Profile*") Profile profile);
    }
}
