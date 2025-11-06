// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.app.NotificationManager;
import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.scheduler.TipsAgent;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Static utilities for Tips Notifications. */
@NullMarked
public class TipsUtils {
    /**
     * Assembles a {@link FeatureTipPromoData} object containing required UI and callback
     * information for the respective {@link TipsNotificationsFeatureType}'s promo'.
     *
     * @param context The Android {@link Context}.
     * @param featureType The {@link TipsNotificationsFeatureType} to show a promo for.
     */
    public static FeatureTipPromoData getFeatureTipPromoDataForType(
            Context context, @TipsNotificationsFeatureType int featureType) {
        final @StringRes int positiveButtonTextRes;
        final @StringRes int mainPageTitleRes;
        final @StringRes int mainPageDescriptionRes;
        final @StringRes int detailPageTitleRes;
        final List<String> detailPageSteps = new ArrayList<>();

        switch (featureType) {
            case TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_esb;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_esb;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_esb;
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_esb));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_second_step_esb));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_third_step_esb));
                break;
            case TipsNotificationsFeatureType.QUICK_DELETE:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_quick_delete;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_quick_delete;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_quick_delete;
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_first_step_quick_delete));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_quick_delete));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_quick_delete));
                break;
            case TipsNotificationsFeatureType.GOOGLE_LENS:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text_lens;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_lens;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_lens;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_lens;
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_lens));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_second_step_lens));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_third_step_lens));
                break;
            case TipsNotificationsFeatureType.BOTTOM_OMNIBOX:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_bottom_omnibox;
                mainPageDescriptionRes =
                        R.string.tips_promo_bottom_sheet_description_bottom_omnibox;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_bottom_omnibox_short;
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_first_step_bottom_omnibox));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_bottom_omnibox));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_bottom_omnibox));
                break;
            default:
                assert false : "Invalid feature type: " + featureType;

                positiveButtonTextRes = Resources.ID_NULL;
                mainPageTitleRes = Resources.ID_NULL;
                mainPageDescriptionRes = Resources.ID_NULL;
                detailPageTitleRes = Resources.ID_NULL;
        }

        return new FeatureTipPromoData(
                context.getString(positiveButtonTextRes),
                context.getString(mainPageTitleRes),
                context.getString(mainPageDescriptionRes),
                context.getString(detailPageTitleRes),
                detailPageSteps);
    }

    /**
     * With a valid profile, schedule a deferred startup task which potentially schedules a feature
     * tip notification based on backend segmentation ranker criteria. Also checks if app-level and
     * tips notifications are also enabled. On all other workflows, ensure that any previously
     * pending notifications are cancelled on either app startup or flag toggle.
     *
     * @param profileProviderSupplier The supplier for the current {@link ProfileProvider}.
     * @param chromeActivityNativeDelegate The current {@link ChromeActivityNativeDelegate}.
     * @param windowAndroid The current {@link WindowAndroid}.
     */
    public static void performNotificationSchedulerSteps(
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            WindowAndroid windowAndroid) {
        profileProviderSupplier.onAvailable(
                (provider) -> {
                    Profile profile = provider.getOriginalProfile();
                    if (profile.shutdownStarted()) return;

                    if (ChromeFeatureList.sAndroidTipsNotifications.isEnabled()) {
                        DeferredStartupHandler.getInstance()
                                .addDeferredTask(
                                        () -> {
                                            if (chromeActivityNativeDelegate
                                                    .isActivityFinishingOrDestroyed()) {
                                                return;
                                            }

                                            maybeScheduleTipsNotification(profile, windowAndroid);
                                        });
                    } else {
                        TipsAgent.removePendingNotifications(profile);
                    }
                });
    }

    private static void maybeScheduleTipsNotification(
            Profile profile, WindowAndroid windowAndroid) {
        boolean isBottomOmnibox = isBottomOmniboxActive(windowAndroid);

        TipsAgent.removePendingNotifications(profile);
        TipsUtils.areTipsNotificationsEnabled(
                (enabled) -> {
                    // If the notification channel is enabled, check if a notification was actually
                    // scheduled before scheduling a task to run the reschedule logic.
                    if (enabled && TipsAgent.maybeScheduleNotification(profile, isBottomOmnibox)) {
                        // Run this current function again in 1 hour since the scheduler will
                        // schedule a notification 4 hours out, so if the user is still active on
                        // Chrome then reschedule it. The remove call earlier in this function will
                        // remove all pending notifications and the new scheduling call acts as a
                        // reschedule. If the app is closed (user offline) with a post delayed task
                        // it will be torn down. Note that it is possible that when the notification
                        // is rescheduled, the usage criteria may have changed such that the user is
                        // no longer eligible to receive a notification.
                        PostTask.postDelayedTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    maybeScheduleTipsNotification(profile, windowAndroid);
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

    private static void areTipsNotificationsEnabled(Callback<Boolean> callback) {
        if (!NotificationProxyUtils.areNotificationsEnabled()) {
            callback.onResult(false);
            return;
        }
        BaseNotificationManagerProxyFactory.create()
                .getNotificationChannel(
                        ChromeChannelDefinitions.ChannelId.TIPS,
                        (channel) -> {
                            if (channel != null
                                    && channel.getImportance()
                                            != NotificationManager.IMPORTANCE_NONE) {
                                callback.onResult(true);
                            } else {
                                callback.onResult(false);
                            }
                        });
    }
}
