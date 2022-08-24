// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_info.SiteSettingsHelper;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabUtils.LoadIfNeededCaller;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Utilities for requesting desktop sites support.
 */
public class RequestDesktopUtils {
    private static final String ANY_SUBDOMAIN_PATTERN = "[*.]";
    private static final String SITE_WILDCARD = "*";

    static final String PARAM_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES =
            "default_on_display_size_threshold_inches";
    static final double DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES = 12.0;
    static final String PARAM_GLOBAL_SETTING_DEFAULT_ON_ON_LOW_END_DEVICES =
            "default_on_on_low_end_devices";
    static final String PARAM_SHOW_MESSAGE_ON_GLOBAL_SETTING_DEFAULT_ON =
            "show_message_on_default_on";

    static final String PARAM_GLOBAL_SETTING_OPT_IN_ENABLED = "show_opt_in_message";
    static final String PARAM_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES =
            "opt_in_display_size_min_threshold_inches";
    static final double DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES = 10.0;
    static final String PARAM_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MAX_THRESHOLD_INCHES =
            "opt_in_display_size_max_threshold_inches";
    static final double DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MAX_THRESHOLD_INCHES =
            Double.MAX_VALUE;

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
     * Records the metrics associated with changing the user agent by user agent.
     * @param isDesktop True if the user agent is the desktop.
     * @param tab The current activity {@link Tab}.
     */
    public static void recordUserChangeUserAgent(boolean isDesktop, @Nullable Tab tab) {
        if (ChromeFeatureList.sAppMenuMobileSiteOption.isEnabled() && !isDesktop) {
            RecordUserAction.record("MobileMenuRequestMobileSite");
        } else {
            RecordUserAction.record("MobileMenuRequestDesktopSite");
        }

        RecordHistogram.recordBooleanHistogram(
                "Android.RequestDesktopSite.UserSwitchToDesktop", isDesktop);

        if (tab == null || tab.isIncognito() || tab.getWebContents() == null) return;

        new UkmRecorder.Bridge().recordEventWithIntegerMetric(tab.getWebContents(),
                "Android.UserRequestedUserAgentChange", "UserAgentType",
                isDesktop ? UserAgentRequestType.REQUEST_DESKTOP
                          : UserAgentRequestType.REQUEST_MOBILE);
    }

    /**
     * Records the ukms associated with changing screen orientation.
     * @param isLandscape True if the orientation is landscape.
     * @param tab The current activity {@link Tab}.
     */
    public static void recordScreenOrientationChangedUkm(boolean isLandscape, @Nullable Tab tab) {
        if (tab == null || tab.isIncognito() || tab.getWebContents() == null) return;

        new UkmRecorder.Bridge().recordEventWithIntegerMetric(tab.getWebContents(),
                "Android.ScreenRotation", "TargetDeviceOrientation",
                isLandscape ? DeviceOrientation2.LANDSCAPE : DeviceOrientation2.PORTRAIT);
    }

    /**
     * Set or remove a domain level exception with URL for {@link
     * ContentSettingsType.REQUEST_DESKTOP_SITE}. Clear the subdomain level exception if any.
     * @param browserContextHandle Target browser context whose content settings needs to be
     *         updated.
     * @param url  {@link GURL} for the site that changes in desktop user agent.
     * @param useDesktopUserAgent True if the input |url| needs to use desktop user agent.
     */
    public static void setRequestDesktopSiteContentSettingsForUrl(
            BrowserContextHandle browserContextHandle, GURL url, boolean useDesktopUserAgent) {
        String domainAndRegistry =
                UrlUtilities.getDomainAndRegistry(url.getSpec(), /*includePrivateRegistries*/ true);
        // Use host only (no scheme/port/path) for ContentSettings to ensure consistency.
        String hostPattern;
        if (TextUtils.isEmpty(domainAndRegistry)) {
            // Use host directly if fails to extract domain from url (e.g. ip address).
            hostPattern = url.getHost();
        } else {
            hostPattern = ANY_SUBDOMAIN_PATTERN + domainAndRegistry;
            // Clear subdomain level exception if any.
            WebsitePreferenceBridge.setContentSettingCustomScope(browserContextHandle,
                    ContentSettingsType.REQUEST_DESKTOP_SITE, url.getHost(),
                    /*secondaryPattern*/ SITE_WILDCARD, ContentSettingValues.DEFAULT);
        }
        @ContentSettingValues
        int defaultValue = WebsitePreferenceBridge.getDefaultContentSetting(
                browserContextHandle, ContentSettingsType.REQUEST_DESKTOP_SITE);
        @ContentSettingValues
        int contentSettingValue;

        assert defaultValue == ContentSettingValues.ALLOW
                || defaultValue == ContentSettingValues.BLOCK;
        boolean blockDesktopGlobally = defaultValue == ContentSettingValues.BLOCK;

        if (useDesktopUserAgent) {
            contentSettingValue = blockDesktopGlobally ? ContentSettingValues.ALLOW
                                                       : ContentSettingValues.DEFAULT;
        } else {
            contentSettingValue = blockDesktopGlobally ? ContentSettingValues.DEFAULT
                                                       : ContentSettingValues.BLOCK;
        }

        // Set or remove a domain level exception.
        WebsitePreferenceBridge.setContentSettingCustomScope(browserContextHandle,
                ContentSettingsType.REQUEST_DESKTOP_SITE, hostPattern,
                /*secondaryPattern*/ SITE_WILDCARD, contentSettingValue);
    }

    /**
     * @param displaySizeInInches The device primary display size, in inches.
     * @return Whether the desktop site global setting should be default-enabled.
     */
    static boolean shouldDefaultEnableGlobalSetting(double displaySizeInInches) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS)) {
            return false;
        }

        // If the device is part of an opt-in experiment arm, avoid default-enabling the setting.
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS,
                    PARAM_GLOBAL_SETTING_OPT_IN_ENABLED, false)) {
            return false;
        }

        // Check whether default-on for low end devices is disabled.
        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS,
                    PARAM_GLOBAL_SETTING_DEFAULT_ON_ON_LOW_END_DEVICES, true)
                && SysUtils.isLowEndDevice()) {
            return false;
        }

        boolean previouslyDefaultEnabled = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, false);
        boolean previouslyUpdatedByUser = SharedPreferencesManager.getInstance().contains(
                SingleCategorySettings.USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY);

        return !previouslyDefaultEnabled && !previouslyUpdatedByUser
                && displaySizeInInches >= ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                           ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS,
                           PARAM_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                           DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES);
    }

    /**
     * Default-enables the desktop site global setting if {@code shouldDefaultEnableGlobalSetting}
     * returns true.
     * @param displaySizeInInches The device primary display size, in inches.
     * @param profile The current {@link Profile}.
     * @return Whether the desktop site global setting was default-enabled.
     */
    public static boolean maybeDefaultEnableGlobalSetting(
            double displaySizeInInches, Profile profile) {
        if (!shouldDefaultEnableGlobalSetting(displaySizeInInches)) {
            return false;
        }

        WebsitePreferenceBridge.setCategoryEnabled(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE, true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, true);
        // This key will be added only once, since this method will be invoked only once on a
        // device. Once the corresponding message is shown, the key will be removed since the
        // message will also be shown at most once.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_SHOW_MESSAGE,
                true);
        return true;
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

        // Present the message only if the global setting has been default-enabled.
        if (!SharedPreferencesManager.getInstance().contains(
                    ChromePreferenceKeys
                            .DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_SHOW_MESSAGE)) {
            return false;
        }

        // Since there might be a delay in triggering this message after the desktop site global
        // setting is default-enabled, it could be possible that the user subsequently disabled the
        // setting. Present the message only if the setting is enabled.
        if (!WebsitePreferenceBridge.isCategoryEnabled(
                    profile, ContentSettingsType.REQUEST_DESKTOP_SITE)) {
            SharedPreferencesManager.getInstance().removeKey(
                    ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_SHOW_MESSAGE);
            return false;
        }

        // Do not show the message if disabled through Finch.
        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS,
                    PARAM_SHOW_MESSAGE_ON_GLOBAL_SETTING_DEFAULT_ON, true)) {
            return false;
        }

        Resources resources = context.getResources();
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.DESKTOP_SITE_GLOBAL_DEFAULT_OPT_OUT)
                        .with(MessageBannerProperties.TITLE,
                                resources.getString(R.string.rds_global_default_on_message_title))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.ic_desktop_windows)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.rds_global_default_on_message_button))
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    SiteSettingsHelper.showCategorySettings(context,
                                            SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .build();

        messageDispatcher.enqueueWindowScopedMessage(message, false);
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_SHOW_MESSAGE);
        return true;
    }

    /**
     * @param displaySizeInInches The device primary display size, in inches.
     * @param profile The current {@link Profile}.
     * @return Whether the message to opt-in to the desktop site global setting should be shown.
     */
    static boolean shouldShowGlobalSettingOptInMessage(
            double displaySizeInInches, Profile profile) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS)) {
            return false;
        }

        // Present the message only if opt-in is enabled.
        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS,
                    PARAM_GLOBAL_SETTING_OPT_IN_ENABLED, false)) {
            return false;
        }

        // Present the message at most once on a device.
        if (SharedPreferencesManager.getInstance().contains(
                    ChromePreferenceKeys.DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_SHOWN)) {
            return false;
        }

        // Present the message only if the user has not previously updated the global setting.
        if (SharedPreferencesManager.getInstance().contains(
                    SingleCategorySettings
                            .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY)) {
            return false;
        }

        // Present the message only if the desktop site global setting is off.
        if (WebsitePreferenceBridge.isCategoryEnabled(
                    profile, ContentSettingsType.REQUEST_DESKTOP_SITE)) {
            return false;
        }

        // Present the message only if the device falls within the range of screen sizes for the
        // opt-in.
        return displaySizeInInches >= ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                       ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS,
                       PARAM_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES,
                       DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES)
                && displaySizeInInches < ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                           ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS,
                           PARAM_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MAX_THRESHOLD_INCHES,
                           DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MAX_THRESHOLD_INCHES);
    }

    /**
     * Creates and shows a message to the user to opt-in to the desktop site global setting based on
     * device conditions.
     * @param displaySizeInInches The device primary display size, in inches.
     * @param profile The current {@link Profile}.
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the message.
     * @param context The current context.
     * @param tab The {@link Tab} where the message is shown.
     * @return Whether the opt-in message was shown.
     */
    public static boolean maybeShowGlobalSettingOptInMessage(double displaySizeInInches,
            Profile profile, MessageDispatcher messageDispatcher, Context context, Tab tab) {
        if (messageDispatcher == null) return false;

        if (!shouldShowGlobalSettingOptInMessage(displaySizeInInches, profile)) {
            return false;
        }

        Resources resources = context.getResources();
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.DESKTOP_SITE_GLOBAL_OPT_IN)
                        .with(MessageBannerProperties.TITLE,
                                resources.getString(R.string.rds_global_opt_in_message_title))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.ic_desktop_windows)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.yes))
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    updateDesktopSiteGlobalSettingOnUserRequest(profile, true);
                                    // TODO(crbug.com/1350274): Remove this explicit load when this
                                    // bug is addressed.
                                    if (tab != null) {
                                        tab.loadIfNeeded(
                                                LoadIfNeededCaller
                                                        .MAYBE_SHOW_GLOBAL_SETTING_OPT_IN_MESSAGE);
                                    }
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .build();

        messageDispatcher.enqueueWindowScopedMessage(message, false);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_SHOWN, true);
        return true;
    }

    /**
     * Show a prompt to educate the user about the update in the behavior of the desktop site app
     * menu setting from a tab-level setting to a site-level setting.
     * @param profile The current {@link Profile}.
     * @return Whether the prompt was shown.
     */
    public static boolean maybeShowUserEducationPromptForAppMenuSelection(Profile profile) {
        if (!TrackerFactory.getTrackerForProfile(profile).shouldTriggerHelpUI(
                    FeatureConstants.REQUEST_DESKTOP_SITE_APP_MENU_FEATURE)) {
            return false;
        }
        // TODO(crbug.com/1353597): Trigger user education dialog for behavior update.
        return true;
    }

    /**
     * Updates the desktop site content setting on user request.
     * @param profile The current {@link Profile}.
     * @param requestDesktopSite Whether the user requested for desktop sites globally.
     */
    static void updateDesktopSiteGlobalSettingOnUserRequest(
            Profile profile, boolean requestDesktopSite) {
        WebsitePreferenceBridge.setCategoryEnabled(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE, requestDesktopSite);
        SingleCategorySettings.recordSiteLayoutChanged(requestDesktopSite);
    }
}