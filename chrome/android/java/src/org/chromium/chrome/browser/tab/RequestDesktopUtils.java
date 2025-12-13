// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.IntDef;
import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.page_info.SiteSettingsHelper;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Utilities for requesting desktop sites support. */
@NullMarked
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

        new UkmRecorder(tab.getWebContents(), "Android.UserRequestedUserAgentChange")
                .addMetric(
                        "UserAgentType",
                        isDesktop
                                ? UserAgentRequestType.REQUEST_DESKTOP
                                : UserAgentRequestType.REQUEST_MOBILE)
                .record();
    }

    /**
     * Records the ukms associated with changing screen orientation.
     *
     * @param isLandscape True if the orientation is landscape.
     * @param tab The current activity {@link Tab}.
     */
    public static void recordScreenOrientationChangedUkm(boolean isLandscape, @Nullable Tab tab) {
        if (tab == null || tab.isOffTheRecord() || tab.getWebContents() == null) return;

        new UkmRecorder(tab.getWebContents(), "Android.ScreenRotation")
                .addMetric(
                        "TargetDeviceOrientation",
                        isLandscape ? DeviceOrientation2.LANDSCAPE : DeviceOrientation2.PORTRAIT)
                .record();
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
        if (DeviceInfo.isDesktop()) {
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
        if (!tracker.shouldTriggerHelpUi(
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
}
