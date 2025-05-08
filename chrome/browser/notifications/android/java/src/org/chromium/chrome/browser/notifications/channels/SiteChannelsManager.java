// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.NotificationSettingsBridge.SiteChannel;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;

import java.util.ArrayList;
import java.util.List;

/** Creates/deletes and queries our notification channels for websites. */
@NullMarked
public class SiteChannelsManager {
    private static final String CHANNEL_ID_PREFIX_SITES = "web:";
    private static final String CHANNEL_ID_SEPARATOR = ";";

    private static @Nullable SiteChannelsManager sInstance;

    private final BaseNotificationManagerProxy mNotificationManagerProxy;

    public static SiteChannelsManager getInstance() {
        if (sInstance == null) {
            sInstance = new SiteChannelsManager();
        }
        return sInstance;
    }

    public static void setInstanceForTesting(SiteChannelsManager instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    private SiteChannelsManager() {
        mNotificationManagerProxy = BaseNotificationManagerProxyFactory.create();
    }

    /**
     * Creates a channel for the given origin. Don't call this if the channel for the origin already
     * exists, as the returned SiteChannel might not have the same description or importance as
     * expected. See:
     * https://developer.android.com/reference/android/app/NotificationManager#createNotificationChannel(android.app.NotificationChannel).
     * The newly created channel will appear within the Sites channel group, with default
     * importance, or no importance if created as blocked.
     *
     * @param origin The site origin, to be used as the channel's user-visible name.
     * @param creationTime A string representing the time of channel creation.
     * @param enabled Determines whether the channel will be created as enabled or blocked.
     * @return The channel created for the given origin.
     */
    public SiteChannel createSiteChannel(String origin, long creationTime, boolean enabled) {
        // Channel group must be created before the channel.
        NotificationChannelGroup channelGroup =
                assumeNonNull(
                                ChromeChannelDefinitions.getInstance()
                                        .getChannelGroup(
                                                ChromeChannelDefinitions.ChannelGroupId.SITES))
                        .toNotificationChannelGroup(
                                ContextUtils.getApplicationContext().getResources());
        mNotificationManagerProxy.createNotificationChannelGroup(channelGroup);
        SiteChannel siteChannel =
                new SiteChannel(
                        createChannelId(origin, creationTime),
                        origin,
                        creationTime,
                        enabled
                                ? NotificationChannelStatus.ENABLED
                                : NotificationChannelStatus.BLOCKED);
        mNotificationManagerProxy.createNotificationChannel(siteChannel.toChannel());
        return siteChannel;
    }

    private void getSiteChannelForOrigin(String origin, Callback<@Nullable SiteChannel> callback) {
        String normalizedOrigin = assumeNonNull(WebsiteAddress.create(origin)).getOrigin();
        getSiteChannelsAsync(
                (siteChannels) -> {
                    for (SiteChannel channel : siteChannels) {
                        if (channel.getOrigin().equals(normalizedOrigin)) {
                            callback.onResult(channel);
                            return;
                        }
                    }
                    callback.onResult(null);
                });
    }

    /** Deletes all site channels. */
    public void deleteAllSiteChannels() {
        mNotificationManagerProxy.deleteAllNotificationChannels(
                channelId -> {
                    return isValidSiteChannelId(channelId);
                });
    }

    /** Deletes the channel associated with this channel ID. */
    public void deleteSiteChannel(String channelId) {
        mNotificationManagerProxy.deleteNotificationChannel(channelId);
    }

    /**
     * Gets the status of the channel associated with this channelId.
     *
     * @param channelId The ID of the notification channel to query
     * @param callback Callback to run after getting the result. The result can be ALLOW, BLOCKED,
     *     or UNAVAILABLE (if the channel was never created or was deleted).
     */
    public void getChannelStatusAsync(String channelId, Callback<Integer> callback) {
        mNotificationManagerProxy.getNotificationChannel(
                channelId,
                (channel) -> {
                    if (channel == null) {
                        callback.onResult(NotificationChannelStatus.UNAVAILABLE);
                        return;
                    }
                    callback.onResult(toChannelStatus(channel.getImportance()));
                });
    }

    /**
     * Gets an array of active site channels (i.e. they have been created on the notification
     * manager). This includes enabled and blocked channels.
     */
    public void getSiteChannelsAsync(Callback<SiteChannel[]> callback) {
        mNotificationManagerProxy.getNotificationChannels(
                (channels) -> {
                    List<SiteChannel> siteChannels = new ArrayList<>();
                    for (NotificationChannel channel : channels) {
                        if (isValidSiteChannelId(channel.getId())) {
                            siteChannels.add(toSiteChannel(channel));
                        }
                    }
                    callback.onResult(siteChannels.toArray(new SiteChannel[siteChannels.size()]));
                });
    }

    private static SiteChannel toSiteChannel(NotificationChannel channel) {
        String originAndTimestamp = channel.getId().substring(CHANNEL_ID_PREFIX_SITES.length());
        String[] parts = originAndTimestamp.split(CHANNEL_ID_SEPARATOR);
        assert parts.length == 2;
        return new SiteChannel(
                channel.getId(),
                parts[0],
                Long.parseLong(parts[1]),
                toChannelStatus(channel.getImportance()));
    }

    public static boolean isValidSiteChannelId(String channelId) {
        return channelId.startsWith(CHANNEL_ID_PREFIX_SITES)
                && channelId
                        .substring(CHANNEL_ID_PREFIX_SITES.length())
                        .contains(CHANNEL_ID_SEPARATOR);
    }

    /** Converts a site's origin and creation timestamp to a canonical channel id. */
    @VisibleForTesting
    public static String createChannelId(String origin, long creationTime) {
        return CHANNEL_ID_PREFIX_SITES
                + assumeNonNull(WebsiteAddress.create(origin)).getOrigin()
                + CHANNEL_ID_SEPARATOR
                + creationTime;
    }

    /**
     * Converts the channel id of a notification channel to a site origin. This is only valid for
     * site notification channels, i.e. channels with ids beginning with {@link
     * CHANNEL_ID_PREFIX_SITES}.
     */
    public static String toSiteOrigin(String channelId) {
        assert channelId.startsWith(CHANNEL_ID_PREFIX_SITES);
        return channelId.substring(CHANNEL_ID_PREFIX_SITES.length()).split(CHANNEL_ID_SEPARATOR)[0];
    }

    /** Converts a notification channel's importance to ENABLED or BLOCKED. */
    private static @NotificationChannelStatus int toChannelStatus(int importance) {
        switch (importance) {
            case NotificationManager.IMPORTANCE_NONE:
                return NotificationChannelStatus.BLOCKED;
            default:
                return NotificationChannelStatus.ENABLED;
        }
    }

    /**
     * Retrieves the notification channel ID for a given origin.
     *
     * @param origin The origin to be queried.
     * @param callback A callback to return the channel ID once the call completes.
     */
    public void getChannelIdForOriginAsync(String origin, Callback<String> callback) {
        getSiteChannelForOrigin(
                origin,
                (channel) -> {
                    // Fall back to generic Sites channel if a channel for this origin doesn't
                    // exist.
                    // TODO(crbug.com/40558363) Stop using this channel as a fallback and fully
                    // deprecate it.
                    if (channel != null) {
                        callback.onResult(channel.getId());
                    } else {
                        RecordHistogram.recordBooleanHistogram(
                                "Notifications.Android.SitesChannel", true);
                        callback.onResult(ChromeChannelDefinitions.ChannelId.SITES);
                    }
                });
    }
}
