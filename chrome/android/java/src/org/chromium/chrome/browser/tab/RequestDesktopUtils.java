// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tab;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Utilities for requesting desktop sites support.
 */
public class RequestDesktopUtils {
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
     * Set or remove a site exception with URL for {@link ContentSettingsType.REQUEST_DESKTOP_SITE}.
     * @param browserContextHandle Target browser context whose content settings needs to be
     *         updated.
     * @param url  {@link GURL} for the site that changes in desktop user agent.
     * @param useDesktopUserAgent True if the input |url| needs to use desktop user agent.
     */
    public static void setRequestDesktopSiteContentSettingsForUrl(
            BrowserContextHandle browserContextHandle, GURL url, boolean useDesktopUserAgent) {
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
        WebsitePreferenceBridge.setContentSettingDefaultScope(browserContextHandle,
                ContentSettingsType.REQUEST_DESKTOP_SITE, url, url, contentSettingValue);
    }
}