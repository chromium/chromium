// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.os.Build;

import dagger.Lazy;

import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.embedder_support.util.Origin;

/**
 * If an origin is associated with an installed webapp (TWAs on Android O+, WebAPKs on Android T+)
 * then we want to remove its Android channel because the APKs notification status takes precedence
 * and we don't want the confuse the user with conflicting UI.
 *
 * <p>It's recommended to hold a {@link Lazy} version of this class and pass this to static methods
 * such as {@link #restoreChannelIfNeeded} to not create instances of this class on Android versions
 * when it is not required.
 *
 * <p>Lifecycle: Singleton. Thread safety: Only call methods on a single thread. Native: Does not
 * require native.
 */
public class NotificationChannelPreserver {
    private NotificationChannelPreserver() {}

    /** Deletes the SiteChannel if called on a version of Android that requires it. */
    static void deleteChannelIfNeeded(Origin origin) {
        if (beforeAndroidO()) return;

        SiteChannelsManager siteChannelsManager = SiteChannelsManager.getInstance();

        String channelId = siteChannelsManager.getChannelIdForOrigin(origin.toString());
        if (ChromeChannelDefinitions.ChannelId.SITES.equals(channelId)) {
            // If we were given the generic "sites" channel that meant no origin-specific channel
            // existed. We don't need to do anything.
            return;
        }

        @NotificationChannelStatus int status = siteChannelsManager.getChannelStatus(channelId);
        if (status == NotificationChannelStatus.UNAVAILABLE) {
            // This shouldn't happen if we passed the above conditional return - but it just means
            // that the channel doesn't exist, so again, we don't need to do anything.
            return;
        }

        assert status == NotificationChannelStatus.ENABLED
                || status == NotificationChannelStatus.BLOCKED;

        @ContentSettingValues
        int settingValue =
                status == NotificationChannelStatus.ENABLED
                        ? ContentSettingValues.ALLOW
                        : ContentSettingValues.BLOCK;
        WebappRegistry.getInstance()
                .getPermissionStore()
                .setPreInstallNotificationPermission(origin, settingValue);
        siteChannelsManager.deleteSiteChannel(channelId);
    }

    /** Restores the SiteChannel if called on a version of Android that requires it. */
    static void restoreChannelIfNeeded(Origin origin) {
        if (beforeAndroidO()) return;

        @ContentSettingValues
        Integer settingValue =
                WebappRegistry.getInstance()
                        .getPermissionStore()
                        .getAndRemovePreInstallNotificationPermission(origin);

        if (settingValue == null) {
            // If no previous value was stored, a channel didn't previously exist.
            return;
        }

        boolean enabled = settingValue == ContentSettingValues.ALLOW;
        SiteChannelsManager.getInstance()
                .createSiteChannel(origin.toString(), System.currentTimeMillis(), enabled);
    }

    private static boolean beforeAndroidO() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.O;
    }
}
