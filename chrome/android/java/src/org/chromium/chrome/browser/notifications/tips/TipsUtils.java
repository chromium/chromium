// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.scheduler.TipsAgent;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Static utilities for Tips Notifications. */
@NullMarked
public class TipsUtils {
    @VisibleForTesting public static final float LOGO_IMAGE_MAX_WIDTH_RATIO = 0.45f;

    // LINT.IfChange(TipsShownPrefs)
    public static final String ENHANCED_SAFE_BROWSING_SHOWN =
            "android.tips.notifications.esb_shown";
    public static final String QUICK_DELETE_SHOWN = "android.tips.notifications.quick_delete_shown";
    public static final String GOOGLE_LENS_SHOWN = "android.tips.notifications.lens_shown";
    public static final String BOTTOM_OMNIBOX_SHOWN =
            "android.tips.notifications.bottom_omnibox_shown";
    public static final String PASSWORD_AUTOFILL_SHOWN =
            "android.tips.notifications.password_autofill_shown";
    public static final String SIGNIN_SHOWN = "android.tips.notifications.signin_shown";
    public static final String CREATE_TAB_GROUP_SHOWN =
            "android.tips.notifications.create_tab_group_shown";
    public static final String CUSTOMIZE_MVT_SHOWN =
            "android.tips.notifications.customize_mvt_shown";
    public static final String RECENT_TABS_SHOWN = "android.tips.notifications.recent_tabs_shown";

    // LINT.ThenChange(//chrome/common/pref_names.h:TipsShownPrefs)

    /**
     * Assembles a {@link FeatureTipPromoData} object containing required UI and callback
     * information for the respective {@link TipsNotificationsFeatureType}'s promo'.
     *
     * @param context The Android {@link Context}.
     * @param featureType The {@link TipsNotificationsFeatureType} to show a promo for.
     * @param isUserSignedIn Whether the user is signed in, used for the Sign-in feature promo.
     */
    public static FeatureTipPromoData getFeatureTipPromoDataForType(
            Context context,
            @TipsNotificationsFeatureType int featureType,
            boolean isUserSignedIn) {
        final @StringRes int positiveButtonTextRes;
        final @StringRes int mainPageTitleRes;
        final @StringRes int mainPageDescriptionRes;
        final int mainPageLogoViewRes;
        final @StringRes int detailPageTitleRes;
        final List<String> detailPageSteps = new ArrayList<>();

        switch (featureType) {
            case TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_esb;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_esb;
                mainPageLogoViewRes = R.drawable.tips_promo_esb_logo;
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
                mainPageLogoViewRes = R.raw.tips_notifications_quick_delete_logo_anim;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_quick_delete_short;
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
                mainPageLogoViewRes = R.drawable.tips_promo_lens_logo;
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
                mainPageLogoViewRes = R.drawable.tips_promo_bottom_omnibox_logo;
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
            case TipsNotificationsFeatureType.PASSWORD_AUTOFILL:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text_noop;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_password_autofill;
                mainPageDescriptionRes =
                        R.string.tips_promo_bottom_sheet_description_password_autofill;
                mainPageLogoViewRes = R.drawable.tips_promo_password_autofill_logo;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_password_autofill;
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_first_step_password_autofill));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_password_autofill));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_password_autofill));
                break;
            case TipsNotificationsFeatureType.SIGNIN:
                positiveButtonTextRes =
                        isUserSignedIn
                                ? R.string.tips_promo_bottom_sheet_positive_button_text_noop
                                : R.string.signin_promo_signin;
                mainPageTitleRes =
                        isUserSignedIn
                                ? R.string.tips_promo_bottom_sheet_title_signin_signed_in
                                : R.string.educational_tip_sign_in_promo_title;
                mainPageDescriptionRes = R.string.educational_tip_sign_in_promo_description;
                mainPageLogoViewRes = R.drawable.tips_promo_signin_logo;
                detailPageTitleRes = R.string.sign_in_to_chrome;
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_signin));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_second_step_signin));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_third_step_signin));
                break;
            case TipsNotificationsFeatureType.CREATE_TAB_GROUPS:
                positiveButtonTextRes =
                        R.string.tips_promo_bottom_sheet_positive_button_text_try_now;
                mainPageTitleRes = R.string.educational_tip_tab_group_title;
                mainPageDescriptionRes = R.string.educational_tip_tab_group_description;
                mainPageLogoViewRes = R.drawable.tips_promo_create_tab_groups_logo;
                detailPageTitleRes = R.string.educational_tip_tab_group_title;
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_first_step_create_tab_groups));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_create_tab_groups));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_create_tab_groups));
                break;
            case TipsNotificationsFeatureType.CUSTOMIZE_MVT:
                positiveButtonTextRes = R.string.tips_promo_bottom_sheet_positive_button_text_noop;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_customize_mvt;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_customize_mvt;
                mainPageLogoViewRes = R.drawable.tips_promo_customize_mvt_logo;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_customize_mvt;
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_first_step_customize_mvt));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_customize_mvt));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_third_step_customize_mvt));
                break;
            case TipsNotificationsFeatureType.RECENT_TABS:
                positiveButtonTextRes =
                        R.string.tips_promo_bottom_sheet_positive_button_text_recent_tabs;
                mainPageTitleRes = R.string.tips_promo_bottom_sheet_title_recent_tabs;
                mainPageDescriptionRes = R.string.tips_promo_bottom_sheet_description_recent_tabs;
                mainPageLogoViewRes = R.drawable.tips_promo_recent_tabs_logo;
                detailPageTitleRes = R.string.tips_promo_bottom_sheet_title_recent_tabs;
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_first_step_recent_tabs));
                detailPageSteps.add(
                        context.getString(
                                R.string.tips_promo_bottom_sheet_second_step_recent_tabs));
                detailPageSteps.add(
                        context.getString(R.string.tips_promo_bottom_sheet_third_step_recent_tabs));
                break;
            default:
                assert false : "Invalid feature type: " + featureType;

                positiveButtonTextRes = Resources.ID_NULL;
                mainPageTitleRes = Resources.ID_NULL;
                mainPageDescriptionRes = Resources.ID_NULL;
                mainPageLogoViewRes = Resources.ID_NULL;
                detailPageTitleRes = Resources.ID_NULL;
        }

        return new FeatureTipPromoData(
                context.getString(positiveButtonTextRes),
                context.getString(mainPageTitleRes),
                context.getString(mainPageDescriptionRes),
                mainPageLogoViewRes,
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
     * @param windowAndroid The current {@link WindowAndroid}.
     */
    public static void performNotificationSchedulerSteps(
            OneshotSupplier<ProfileProvider> profileProviderSupplier, WindowAndroid windowAndroid) {
        profileProviderSupplier.onAvailable(
                (provider) -> {
                    Profile profile = provider.getOriginalProfile();
                    if (profile.shutdownStarted()) return;

                    if (ChromeFeatureList.sAndroidTipsNotifications.isEnabled()) {
                        if (ChromeFeatureList.sAndroidTipsNotificationsResetFeatureTipShown
                                .getValue()) {
                            clearFeatureTipShownPrefs(profile);
                        }

                        maybeScheduleTipsNotification(profile, windowAndroid);
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
                    if (enabled) {
                        TipsAgent.maybeScheduleNotification(profile, isBottomOmnibox);
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

    /**
     * Check if both app-level and tips notifications are enabled.
     *
     * @param callback Callback to return the result.
     */
    public static void areTipsNotificationsEnabled(Callback<Boolean> callback) {
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

    /**
     * Launch the settings page for the Tips Notifications channel.
     *
     * @param context The current context.
     */
    public static void launchTipsNotificationsSettings(Context context) {
        // Make sure the channel is initialized before sending users to the settings.
        createNotificationChannel(context);
        context.startActivity(getNotificationSettingsIntent(context));
    }

    private static Intent getNotificationSettingsIntent(Context context) {
        Intent intent = new Intent();
        if (areAppNotificationsEnabled()) {
            intent.setAction(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
            intent.putExtra(Settings.EXTRA_CHANNEL_ID, ChromeChannelDefinitions.ChannelId.TIPS);
        } else {
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    private static boolean areAppNotificationsEnabled() {
        return NotificationProxyUtils.areNotificationsEnabled();
    }

    private static void createNotificationChannel(Context context) {
        new ChannelsInitializer(
                        BaseNotificationManagerProxyFactory.create(),
                        ChromeChannelDefinitions.getInstance(),
                        context.getResources())
                .ensureInitialized(ChromeChannelDefinitions.ChannelId.TIPS);
    }

    /**
     * @return Whether the opt-in promo should be always shown for testing tips.
     */
    public static boolean shouldAlwaysShowOptInPromo() {
        return ChromeFeatureList.sAndroidTipsNotificationsAlwaysShowOptInPromo.getValue();
    }

    /**
     * Set whether the tips notifications channel is enabled for use in the tips notifications magic
     * stack module for show eligibility checking.
     */
    public static void registerTipsNotificationsModuleEnabledSettingsPref() {
        areTipsNotificationsEnabled(
                (enabled) -> {
                    ChromeSharedPreferences.getInstance()
                            .writeBoolean(
                                    ChromePreferenceKeys.TIPS_NOTIFICATIONS_CHANNEL_ENABLED,
                                    enabled);
                });
    }

    private static void clearFeatureTipShownPrefs(Profile profile) {
        UserPrefs.get(profile).setBoolean(ENHANCED_SAFE_BROWSING_SHOWN, false);
        UserPrefs.get(profile).setBoolean(QUICK_DELETE_SHOWN, false);
        UserPrefs.get(profile).setBoolean(GOOGLE_LENS_SHOWN, false);
        UserPrefs.get(profile).setBoolean(BOTTOM_OMNIBOX_SHOWN, false);
        UserPrefs.get(profile).setBoolean(PASSWORD_AUTOFILL_SHOWN, false);
        UserPrefs.get(profile).setBoolean(SIGNIN_SHOWN, false);
        UserPrefs.get(profile).setBoolean(CREATE_TAB_GROUP_SHOWN, false);
        UserPrefs.get(profile).setBoolean(CUSTOMIZE_MVT_SHOWN, false);
        UserPrefs.get(profile).setBoolean(RECENT_TABS_SHOWN, false);
    }

    /**
     * Scale the image logo for the bottom sheet by width based on orientation. The bottom sheet
     * logo images are designed for portrait mode to match parent width and when scaled for
     * landscape mode/split screen are too large. A ratio is applied based on the screen width to
     * scale it down to a reasonable size.
     *
     * @param context The current context.
     * @param configuration The current configuration of the device.
     * @param contentView The content view of the bottom sheet that holds the image.
     * @param logoRes The resource id of the image logo to be scaled.
     */
    public static void scaleBottomSheetImageLogoByWidth(
            Context context, Configuration configuration, View contentView, @IdRes int logoRes) {
        // While the logic does not need to be applied to LFFs, detection for tablets is
        // dependent on minimum width which is the same as the detection for landscape
        // mode and exclusion would nullify the effects, so this applies to all devices.
        int screenWidthDp = configuration.screenWidthDp;
        int screenWidthPixels = ViewUtils.dpToPx(context, screenWidthDp);
        ImageView logoView = contentView.findViewById(logoRes);
        ViewGroup.LayoutParams layoutParams = logoView.getLayoutParams();

        layoutParams.width =
                configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
                        ? Math.round(screenWidthPixels * LOGO_IMAGE_MAX_WIDTH_RATIO)
                        : ViewGroup.LayoutParams.MATCH_PARENT;
        logoView.setLayoutParams(layoutParams);
    }

    /**
     * Get the background drawable resource for a step in the detail page based on its position in
     * the list.
     *
     * @param stepIndex The index of the step in the list.
     * @param stepCount The total number of steps in the list.
     * @return The background drawable resource id.
     */
    public static @DrawableRes int getDetailStepBackground(int stepIndex, int stepCount) {
        if (stepCount == 1) {
            return R.drawable.view_list_single_item_background;
        }
        if (stepIndex == 0) {
            return R.drawable.view_list_top_item_background;
        }
        if (stepIndex == stepCount - 1) {
            return R.drawable.view_list_bottom_item_background;
        }
        return R.drawable.view_list_normal_item_background;
    }

    /**
     * Assembles a {@link BottomSheetSigninAndHistorySyncConfig} for the account picker bottom sheet
     * used in the Sign-in feature flow.
     *
     * @param context The current context.
     */
    public static BottomSheetSigninAndHistorySyncConfig getAccountPickerBottomSheetConfig(
            Context context) {
        String title = context.getString(R.string.signin_account_picker_bottom_sheet_title);
        String subtitle =
                context.getString(R.string.signin_account_picker_bottom_sheet_benefits_subtitle);
        AccountPickerBottomSheetStrings accountPickerBottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(title)
                        .setSubtitleString(subtitle)
                        .build();
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                accountPickerBottomSheetStrings,
                                BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode
                                        .BOTTOM_SHEET,
                                BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.OPTIONAL,
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .build();
        return config;
    }
}
