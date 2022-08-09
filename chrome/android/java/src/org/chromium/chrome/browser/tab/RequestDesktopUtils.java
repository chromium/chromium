// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tab;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.BrowserContextHandle;
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
    public static boolean shouldDefaultEnableGlobalSetting(double displaySizeInInches) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS)) {
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
        return true;
    }
}