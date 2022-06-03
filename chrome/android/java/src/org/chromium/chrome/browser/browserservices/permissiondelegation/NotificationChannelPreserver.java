// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.os.Build;

import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.components.embedder_support.util.Origin;

import javax.inject.Inject;
import javax.inject.Singleton;

import dagger.Lazy;

/**
 * If an origin is associated with a TWA on Android O+, we want to remove its Android channel
 * because the TWA's notification status takes precedence and we don't want the confuse the user
 * with conflicting UI.
 *
 * It's recommended to hold a {@link Lazy} version of this class and pass this to static methods
 * such as {@link #restoreChannelIfNeeded} to not create instances of this class on Android versions
 * when it is not required.
 *
 * Lifecycle: Singleton.
 * Thread safety: Only call methods on a single thread.
 * Native: Does not require native.
 */
@Singleton
public class NotificationChannelPreserver {
    private final TrustedWebActivityPermissionStore mStore;
    private final SiteChannelsManager mSiteChannelsManager;

    @Inject
    NotificationChannelPreserver(TrustedWebActivityPermissionStore store,
            SiteChannelsManager siteChannelsManager) {
        assert !beforeAndroidO()
                : "This class should not be instantiated on Android versions before O";

        mStore = store;
        mSiteChannelsManager = siteChannelsManager;
    }

    void deleteChannel(Origin origin) {
        if (beforeAndroidO()) return;

        String channelId = mSiteChannelsManager.getChannelIdForOrigin(origin.toString());
        if (ChromeChannelDefinitions.ChannelId.SITES.equals(channelId)) {
            // If we were given the generic "sites" channel that meant no origin-specific channel
            // existed. We don't need to do anything.
            return;
        }

        @NotificationChannelStatus int status = mSiteChannelsManager.getChannelStatus(channelId);
        if (status == NotificationChannelStatus.UNAVAILABLE) {
            // This shouldn't happen if we passed the above conditional return - but it just means
            // that the channel doesn't exist, so again, we don't need to do anything.
            return;
        }

        assert status == NotificationChannelStatus.ENABLED ||
                status == NotificationChannelStatus.BLOCKED;
        mStore.setPreTwaNotificationState(origin, status == NotificationChannelStatus.ENABLED);
        mSiteChannelsManager.deleteSiteChannel(channelId);
    }

    void restoreChannel(Origin origin) {
        if (beforeAndroidO()) return;

        Boolean enabled = mStore.getPreTwaNotificationState(origin);

        if (enabled == null) {
            // If no previous channel status was stored, a channel didn't previously exist.
            return;
        }

        mSiteChannelsManager.createSiteChannel(
                origin.toString(), System.currentTimeMillis(), enabled);
    }

    /**
     * Resolves the {@link Lazy} {@code preserver} and calls {@link #deleteChannel} on it if called
     * on a version of Android that requires it. Does not resolve the {@code preserver} otherwise.
     */
    static void deleteChannelIfNeeded(Lazy<NotificationChannelPreserver> preserver, Origin origin) {
        if (beforeAndroidO()) return;
        preserver.get().deleteChannel(origin);
    }

    /**
     * Similar to {@link #deleteChannelIfNeeded}, but calling {@link #restoreChannel}.
     */
    static void restoreChannelIfNeeded(
            Lazy<NotificationChannelPreserver> preserver, Origin origin) {
        if (beforeAndroidO()) return;
        preserver.get().restoreChannel(origin);
    }

    private static boolean beforeAndroidO() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.O;
    }
}
