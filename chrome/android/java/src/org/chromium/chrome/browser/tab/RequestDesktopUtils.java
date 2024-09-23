// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tab;

import static org.chromium.components.content_settings.PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.page_info.SiteSettingsHelper;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/** Utilities for requesting desktop sites support. */
public class RequestDesktopUtils {
    private static final String SITE_WILDCARD = "*";
    // Global defaults experiment constants.
    private static DisplayMetrics sDisplayMetrics;

    static final double DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES = 10.0;
    static final int DEFAULT_GLOBAL_SETTING_DEFAULT_ON_SMALLEST_SCREEN_WIDTH_THRESHOLD_DP = 600;
    static final int DEFAULT_GLOBAL_SETTING_DEFAULT_ON_MEMORY_LIMIT_THRESHOLD_MB = 6500;

    // Note: these values must match the UserAgentRequestType enum in enums.xml.
    @IntDef({UserAgentRequestType.REQUEST_DESKTOP, UserAgentRequestType.REQUEST_MOBILE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface UserAgentRequestType {
        int REQUEST_DESKTOP = 0;
        int REQUEST_MOBILE = 1;
    }

    // Note: these values must match the DeviceOrientation2 enum in enums.xml.
    @IntDef({DeviceOrientation2.LANDSCAPE, DeviceOrientation2.PORTRAIT})
    @Retention(RetentionPolicy.SOURCE)
    private @interface DeviceOrientation2 {
        int LANDSCAPE = 0;
        int PORTRAIT = 1;
    }

    /**
     * Records the metrics associated with changing the user agent by user.
     *
     * @param isDesktop True if the user agent is the desktop.
     * @param tab The current activity {@link Tab}.
     */
    public static void recordUserChangeUserAgent(boolean isDesktop, @Nullable Tab tab) {
        RecordUserAction.record("MobileMenuRequestDesktopSite");

        RecordHistogram.recordBooleanHistogram(
                "Android.RequestDesktopSite.UserSwitchToDesktop", isDesktop);

        if (tab == null || tab.isOffTheRecord() || tab.getWebContents() == null) return;

        new UkmRecorder.Bridge()
                .recordEventWithIntegerMetric(
                        tab.getWebContents(),
                        "Android.UserRequestedUserAgentChange",
                        "UserAgentType",
                        isDesktop
                                ? UserAgentRequestType.REQUEST_DESKTOP
                                : UserAgentRequestType.REQUEST_MOBILE);
    }

    /**
     * Records the ukms associated with changing screen orientation.
     *
     * @param isLandscape True if the orientation is landscape.
     * @param tab The current activity {@link Tab}.
     */
    public static void recordScreenOrientationChangedUkm(boolean isLandscape, @Nullable Tab tab) {
        if (tab == null || tab.isOffTheRecord() || tab.getWebContents() == null) return;

        new UkmRecorder.Bridge()
                .recordEventWithIntegerMetric(
                        tab.getWebContents(),
                        "Android.ScreenRotation",
                        "TargetDeviceOrientation",
                        isLandscape ? DeviceOrientation2.LANDSCAPE : DeviceOrientation2.PORTRAIT);
    }

    /**
     * Set or remove a domain level exception with URL for {@link
     * ContentSettingsType.REQUEST_DESKTOP_SITE}. Clear the subdomain level exception if any.
     *
     * @param profile Target profile whose content settings needs to be updated.
     * @param url {@link GURL} for the site that changes in desktop user agent.
     * @param useDesktopUserAgent True if the input |url| needs to use desktop user agent.
     */
    public static void setRequestDesktopSiteContentSettingsForUrl(
            Profile profile, GURL url, boolean useDesktopUserAgent) {
        boolean isOffTheRecord = profile.isOffTheRecord();
        String domainWildcardPattern =
                WebsitePreferenceBridge.toDomainWildcardPattern(url.getSpec());
        // Clear subdomain level exception if any.
        WebsitePreferenceBridge.setContentSettingCustomScope(
                profile,
                ContentSettingsType.REQUEST_DESKTOP_SITE,
                url.getHost(),
                /* secondaryPattern= */ SITE_WILDCARD,
                ContentSettingValues.DEFAULT);

        @ContentSettingValues
        int defaultValue =
                WebsitePreferenceBridge.getDefaultContentSetting(
                        profile, ContentSettingsType.REQUEST_DESKTOP_SITE);
        assert defaultValue == ContentSettingValues.ALLOW
                || defaultValue == ContentSettingValues.BLOCK;
        boolean rdsGlobalSetting = defaultValue == ContentSettingValues.ALLOW;
        @ContentSettingValues
        int contentSettingValue =
                useDesktopUserAgent ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;
        // For normal profile, remove domain level setting if it matches the global setting.
        // For incognito profile, keep the domain level setting to override the settings from normal
        // profile.
        if (!isOffTheRecord && useDesktopUserAgent == rdsGlobalSetting) {
            // Keep the domain settings when the window setting preference is ON.
            PrefService prefService = UserPrefs.get(profile);
            if (!prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED)) {
                contentSettingValue = ContentSettingValues.DEFAULT;
            }
        }

        // Set or remove a domain level exception.
        WebsitePreferenceBridge.setContentSettingCustomScope(
                profile,
                ContentSettingsType.REQUEST_DESKTOP_SITE,
                domainWildcardPattern,
                /* secondaryPattern= */ SITE_WILDCARD,
                contentSettingValue);
    }

    /**
     * Upgrade a non-default tab level RDS setting to a domain level setting when RDS exceptions is
     * supported. This method is expected to be invoked only once after support is added for domain
     * level exceptions.
     * @param tab The {@link Tab} for which the RDS setting will be upgraded.
     * @param profile The {@link Profile} used to upgrade the RDS setting.
     * @param tabUserAgent The current {@link TabUserAgent} set for the tab.
     * @param url The {@link GURL} for which a domain level exception will be added.
     */
    public static void maybeUpgradeTabLevelDesktopSiteSetting(
            Tab tab, Profile profile, @TabUserAgent int tabUserAgent, @Nullable GURL url) {
        if (url == null) {
            return;
        }

        // If the tab UA is UNSET, it represents a state before tab level settings were applied for
        // the tab, so the domain level setting cannot be upgraded to at this time.
        if (tabUserAgent == TabUserAgent.UNSET) {
            return;
        }

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(
                profile, url, tabUserAgent == TabUserAgent.DESKTOP);
        // Reset the tab level setting after upgrade.
        tab.setUserAgent(TabUserAgent.DEFAULT);
    }

    /**
     * Determines whether the desktop site global setting should be enabled by default.
     *
     * @param displaySizeInInches The device primary display size, in inches.
     * @param context The current context.
     * @return Whether the desktop site global setting should be default-enabled.
     */
    static boolean shouldDefaultEnableGlobalSetting(double displaySizeInInches, Context context) {
        // Desktop Android always requests desktop sites.
        if (BuildConfig.IS_DESKTOP_ANDROID) {
            return true;
        }

        // Do not default-enable if memory is below threshold.
        if (SysUtils.amountOfPhysicalMemoryKB()
                < DEFAULT_GLOBAL_SETTING_DEFAULT_ON_MEMORY_LIMIT_THRESHOLD_MB
                        * ConversionUtils.KILOBYTES_PER_MEGABYTE) {
            return false;
        }

        // Do not default-enable for x86 devices.
        if (!isCpuArchitectureArm()) {
            return false;
        }

        // Do not default-enable on external display.
        if (isOnExternalDisplay(context)) {
            return false;
        }

        // Do not default-enable if the screen size in inches is below threshold.
        if (displaySizeInInches < DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES) {
            return false;
        }

        // Do not default-enable if the smallest screen size in dp is below threshold.
        if (context.getResources().getConfiguration().smallestScreenWidthDp
                < DEFAULT_GLOBAL_SETTING_DEFAULT_ON_SMALLEST_SCREEN_WIDTH_THRESHOLD_DP) {
            return false;
        }

        // Do not default-enable if the setting has already been default enabled; or the user has
        // explicitly updated the setting.
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        boolean previouslyDefaultEnabled =
                sharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, false);
        boolean previouslyUpdatedByUser =
                sharedPreferencesManager.contains(
                        SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY);
        return !previouslyDefaultEnabled && !previouslyUpdatedByUser;
    }

    /**
     * Default-enables the desktop site global setting if {@code shouldDefaultEnableGlobalSetting}
     * returns true.
     * @param displaySizeInInches The device primary display size, in inches.
     * @param profile The current {@link Profile}.
     * @param context The current context.
     * @return Whether the desktop site global setting was default-enabled.
     */
    public static boolean maybeDefaultEnableGlobalSetting(
            double displaySizeInInches, Profile profile, Context context) {
        if (!shouldDefaultEnableGlobalSetting(displaySizeInInches, context)) {
            return false;
        }

        WebsitePreferenceBridge.setCategoryEnabled(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, true);
        return true;
    }

    /**
     * Default-enables the desktop site window setting if Chrome is opened on a tablet-sized
     * internal display.
     *
     * @param activity The current {@link Activity}.
     * @param profile The current {@link Profile}.
     */
    public static void maybeDefaultEnableWindowSetting(Activity activity, Profile profile) {
        int smallestScreenWidthDp = DisplayUtil.getCurrentSmallestScreenWidth(activity);
        boolean isOnExternalDisplay = isOnExternalDisplay(activity);
        if (isOnExternalDisplay
                || smallestScreenWidthDp < DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP
                || BuildConfig.IS_DESKTOP_ANDROID) {
            return;
        }
        PrefService prefService = UserPrefs.get(profile);
        if (prefService.isDefaultValuePreference(DESKTOP_SITE_WINDOW_SETTING_ENABLED)) {
            prefService.setBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED, /* newValue= */ true);
        }
    }

    /**
     * Creates and shows a message to notify the user of a default update to the desktop site global
     * setting.
     * @param profile The current {@link Profile}.
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the message.
     * @param context The current context.
     * @return Whether the message was shown.
     */
    public static boolean maybeShowDefaultEnableGlobalSettingMessage(
            Profile profile, MessageDispatcher messageDispatcher, Context context) {
        if (messageDispatcher == null) return false;

        // Desktop devices always request desktop sites so there's no need to show a message to
        // the user.
        if (BuildConfig.IS_DESKTOP_ANDROID) {
            return false;
        }

        // Present the message only if the global setting has been default-enabled.
        if (!ChromeSharedPreferences.getInstance()
                .contains(ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING)) {
            return false;
        }

        // Since there might be a delay in triggering this message after the desktop site global
        // setting is default-enabled, it could be possible that the user subsequently disabled the
        // setting. Present the message only if the setting is enabled.
        if (!WebsitePreferenceBridge.isCategoryEnabled(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE)) {
            return false;
        }

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.shouldTriggerHelpUI(
                FeatureConstants.REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE)) {
            return false;
        }

        Resources resources = context.getResources();
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.DESKTOP_SITE_GLOBAL_DEFAULT_OPT_OUT)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(R.string.rds_global_default_on_message_title))
                        .with(
                                MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.ic_desktop_windows)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.rds_global_default_on_message_button))
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    SiteSettingsHelper.showCategorySettings(
                                            context,
                                            SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE);
                                    tracker.notifyEvent(
                                            EventConstants.DESKTOP_SITE_DEFAULT_ON_PRIMARY_ACTION);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                (dismissReason) -> {
                                    if (dismissReason == DismissReason.GESTURE) {
                                        tracker.notifyEvent(
                                                EventConstants.DESKTOP_SITE_DEFAULT_ON_GESTURE);
                                    }
                                    tracker.dismissed(
                                            FeatureConstants
                                                    .REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE);
                                })
                        .build();

        messageDispatcher.enqueueWindowScopedMessage(message, false);
        return true;
    }

    /** Record event for feature engagement on desktop site settings page open. */
    public static void notifyRequestDesktopSiteSettingsPageOpened(Profile profile) {
        TrackerFactory.getTrackerForProfile(profile)
                .notifyEvent(EventConstants.DESKTOP_SITE_SETTINGS_PAGE_OPENED);
    }

    /**
     * Determine whether RDS window setting should be applied. When returning 'true' the mobile user
     * agent should be used for the current window size.
     */
    static boolean shouldApplyWindowSetting(Profile profile, GURL url, Context context) {
        // Skip window setting on Automotive and revisit if / when they add split screen.
        if (BuildInfo.getInstance().isAutomotive) {
            return false;
        }
        PrefService prefService = UserPrefs.get(profile);
        if (!prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED)) {
            return false;
        }
        if (!TabUtils.isRequestDesktopSiteContentSettingsGlobal(profile, url)) {
            return false;
        }
        // Try the window attributes width first.
        // PCCT has its width stored in window attributes.
        int widthPixels = -1;
        Activity activity = ContextUtils.activityFromContext(context);
        // activity might be null in integration tests.
        if (activity != null && activity.getWindow() != null) {
            widthPixels = activity.getWindow().getAttributes().width;
        }
        DisplayMetrics displayMetrics = RequestDesktopUtils.getDisplayMetricsFromContext(context);
        // Use width from displayMetrics if the window attributes width is invalid.
        if (widthPixels <= 0) {
            widthPixels = displayMetrics.widthPixels;
        }
        return widthPixels / displayMetrics.density < DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
    }

    /**
     * Retrieve the {@link DisplayMetrics} from {@link Context} for the current window.
     * @param context The current {@link Context}.
     * @return The {@link DisplayMetrics} for the current window.
     */
    private static DisplayMetrics getDisplayMetricsFromContext(Context context) {
        if (sDisplayMetrics != null) {
            return sDisplayMetrics;
        }
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        display.getMetrics(displayMetrics);
        return displayMetrics;
    }

    @VisibleForTesting
    static void setTestDisplayMetrics(DisplayMetrics displayMetrics) {
        sDisplayMetrics = displayMetrics;
    }

    /** Check if the CPU architecture is ARM. */
    private static boolean isCpuArchitectureArm() {
        String[] abiStrings = Build.SUPPORTED_ABIS;
        if (abiStrings == null || abiStrings.length == 0) {
            return false;
        }
        return abiStrings[0].toLowerCase(Locale.ROOT).contains("arm");
    }

    private static boolean isOnExternalDisplay(Context context) {
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        return display.getDisplayId() != Display.DEFAULT_DISPLAY;
    }
}
