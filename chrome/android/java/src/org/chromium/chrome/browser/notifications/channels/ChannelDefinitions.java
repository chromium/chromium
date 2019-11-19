// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import android.annotation.TargetApi;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;
import android.content.res.Resources;
import android.os.Build;

import androidx.annotation.StringDef;

import org.chromium.chrome.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Contains the properties of all our pre-definable notification channels on Android O+. In practice
 * this is all our channels except site channels, which are defined dynamically by the
 * {@link SiteChannelsManager}. <br/>
 * <br/>
 * PLEASE NOTE: Notification channels appear in system UI and their properties are persisted forever
 * by Android, so should not be added or removed lightly, and the proper deprecation and versioning
 * steps must be taken when doing so. <br/>
 * <br/>
 * See the README.md in this directory for more information before adding or changing any channels.
 */
@TargetApi(Build.VERSION_CODES.O)
public class ChannelDefinitions {
    public static final String CHANNEL_ID_PREFIX_SITES = "web:";
    /**
     * Version number identifying the current set of channels. This must be incremented whenever the
     * set of channels returned by {@link #getStartupChannelIds()} or {@link #getLegacyChannelIds()}
     * changes.
     */
    static final int CHANNELS_VERSION = 2;

    /**
     * To define a new channel, add the channel ID to this StringDef and add a new entry to
     * PredefinedChannels.MAP below with the appropriate channel parameters. To remove an existing
     * channel, remove the ID from this StringDef, remove its entry from Predefined Channels.MAP,
     * and add the ID to the LEGACY_CHANNELS_ID array below. See the README in this directory for
     * more detailed instructions.
     */
    @StringDef({ChannelId.BROWSER, ChannelId.DOWNLOADS, ChannelId.INCOGNITO, ChannelId.MEDIA,
            ChannelId.SCREEN_CAPTURE, ChannelId.CONTENT_SUGGESTIONS, ChannelId.WEBAPP_ACTIONS,
            ChannelId.SITES, ChannelId.SHARING, ChannelId.UPDATES, ChannelId.COMPLETED_DOWNLOADS,
            ChannelId.PERMISSION_REQUESTS, ChannelId.PERMISSION_REQUESTS_HIGH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChannelId {
        String BROWSER = "browser";
        String DOWNLOADS = "downloads";
        String INCOGNITO = "incognito";
        String MEDIA = "media";
        String SCREEN_CAPTURE = "screen_capture";
        String CONTENT_SUGGESTIONS = "content_suggestions";
        String WEBAPP_ACTIONS = "webapp_actions";
        // TODO(crbug.com/700377): Deprecate the 'sites' channel.
        String SITES = "sites";
        String VR = "vr";
        String SHARING = "sharing";
        String UPDATES = "updates";
        String COMPLETED_DOWNLOADS = "completed_downloads";
        String PERMISSION_REQUESTS = "permission_requests";
        String PERMISSION_REQUESTS_HIGH = "permission_requests_high";
    }

    @StringDef({
            ChannelGroupId.GENERAL, ChannelGroupId.SITES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChannelGroupId {
        String SITES = "sites";
        String GENERAL = "general";
    }

    // Map defined in static inner class so it's only initialized lazily.
    @TargetApi(Build.VERSION_CODES.N) // for NotificationManager.IMPORTANCE_* constants
    private static class PredefinedChannels {
        /**
         * Definitions for predefined channels. Any channel listed in STARTUP must have an entry in
         * this map; it may also contain channels that are enabled conditionally.
         */
        static final Map<String, PredefinedChannel> MAP;

        /**
         * The set of predefined channels to be initialized on startup.
         * <p>
         * CHANNELS_VERSION must be incremented every time an entry is added to or removed from this
         * set, or when the definition of one of a channel in this set is changed. If an entry is
         * removed from here then it must be added to the LEGACY_CHANNEL_IDs array.
         */
        static final Set<String> STARTUP;

        static {
            Map<String, PredefinedChannel> map = new HashMap<>();
            Set<String> startup = new HashSet<>();

            map.put(ChannelId.BROWSER,
                    new PredefinedChannel(ChannelId.BROWSER, R.string.notification_category_browser,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.GENERAL));
            startup.add(ChannelId.BROWSER);

            map.put(ChannelId.DOWNLOADS,
                    new PredefinedChannel(ChannelId.DOWNLOADS,
                            R.string.notification_category_downloads,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.GENERAL));
            startup.add(ChannelId.DOWNLOADS);

            map.put(ChannelId.INCOGNITO,
                    new PredefinedChannel(ChannelId.INCOGNITO,
                            R.string.notification_category_incognito,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.GENERAL));
            startup.add(ChannelId.INCOGNITO);

            map.put(ChannelId.MEDIA,
                    new PredefinedChannel(ChannelId.MEDIA, R.string.notification_category_media,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.GENERAL));
            startup.add(ChannelId.MEDIA);

            // ChannelId.SCREEN_CAPTURE will be created on first use, instead of on startup,
            // so that it doesn't clutter the list for users who don't use this feature.
            map.put(ChannelId.SCREEN_CAPTURE,
                    new PredefinedChannel(ChannelId.SCREEN_CAPTURE,
                            R.string.notification_category_screen_capture,
                            NotificationManager.IMPORTANCE_HIGH, ChannelGroupId.GENERAL));

            map.put(ChannelId.SHARING,
                    new PredefinedChannel(ChannelId.SHARING, R.string.notification_category_sharing,
                            NotificationManager.IMPORTANCE_HIGH, ChannelGroupId.GENERAL));
            // Not adding sites channel to startup channels because notifications may be posted to
            // this channel if no site-specific channel could be found.
            // TODO(crbug.com/802380) Stop using this channel as a fallback and fully deprecate it.
            map.put(ChannelId.SITES,
                    new PredefinedChannel(ChannelId.SITES, R.string.notification_category_sites,
                            NotificationManager.IMPORTANCE_DEFAULT, ChannelGroupId.GENERAL));

            // Not adding to startup channels because this channel is experimental and enabled only
            // through the associated feature (see
            // org.chromium.chrome.browser.ntp.ContentSuggestionsNotificationHelper).
            map.put(ChannelId.CONTENT_SUGGESTIONS,
                    new PredefinedChannel(ChannelId.CONTENT_SUGGESTIONS,
                            R.string.notification_category_content_suggestions,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.GENERAL));

            // Not adding to startup channels because we want ChannelId.WEBAPP_ACTIONS to be
            // created on the first use, as not all users use installed web apps.
            map.put(ChannelId.WEBAPP_ACTIONS,
                    new PredefinedChannel(ChannelId.WEBAPP_ACTIONS,
                            R.string.notification_category_fullscreen_controls,
                            NotificationManager.IMPORTANCE_MIN, ChannelGroupId.GENERAL));

            // Not adding to startup channels because we want ChannelId.VR to be created on the
            // first use, as not all users use VR. Channel must have high importance for
            // notifications to show up in VR.
            map.put(ChannelId.VR,
                    new PredefinedChannel(ChannelId.VR, R.string.notification_category_vr,
                            NotificationManager.IMPORTANCE_HIGH, ChannelGroupId.GENERAL));

            map.put(ChannelId.UPDATES,
                    new PredefinedChannel(ChannelId.UPDATES, R.string.notification_category_updates,
                            NotificationManager.IMPORTANCE_HIGH, ChannelGroupId.GENERAL));

            map.put(ChannelId.COMPLETED_DOWNLOADS,
                    new PredefinedChannel(ChannelId.COMPLETED_DOWNLOADS,
                            R.string.notification_category_completed_downloads,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.GENERAL,
                            true /* showNotificationBadges */));

            map.put(ChannelId.PERMISSION_REQUESTS,
                    new PredefinedChannel(ChannelId.PERMISSION_REQUESTS,
                            R.string.notification_category_permission_requests,
                            NotificationManager.IMPORTANCE_LOW, ChannelGroupId.GENERAL));

            map.put(ChannelId.PERMISSION_REQUESTS_HIGH,
                    new PredefinedChannel(ChannelId.PERMISSION_REQUESTS_HIGH,
                            R.string.notification_category_permission_requests,
                            NotificationManager.IMPORTANCE_HIGH, ChannelGroupId.GENERAL));

            MAP = Collections.unmodifiableMap(map);
            STARTUP = Collections.unmodifiableSet(startup);
        }
    }

    /**
     * When channels become deprecated they should be removed from PredefinedChannels and their ids
     * added to this array so they can be deleted on upgrade. We also want to keep track of old
     * channel ids so they aren't accidentally reused.
     */
    private static final String[] LEGACY_CHANNEL_IDS = {
            ChannelDefinitions.ChannelId.SITES
    };

    // Map defined in static inner class so it's only initialized lazily.
    private static class PredefinedChannelGroups {
        static final Map<String, PredefinedChannelGroup> MAP;
        static {
            Map<String, PredefinedChannelGroup> map = new HashMap<>();
            map.put(ChannelGroupId.GENERAL,
                    new PredefinedChannelGroup(
                            ChannelGroupId.GENERAL, R.string.notification_category_group_general));
            map.put(ChannelGroupId.SITES,
                    new PredefinedChannelGroup(
                            ChannelGroupId.SITES, R.string.notification_category_sites));
            MAP = Collections.unmodifiableMap(map);
        }
    }

    /**
     * @return A set of all known channel group ids that can be used for {@link #getChannelGroup}.
     */
    static Set<String> getAllChannelGroupIds() {
        return PredefinedChannelGroups.MAP.keySet();
    }

    /**
     * @return A set of all known channel ids that can be used for {@link #getChannelFromId}.
     */
    static Set<String> getAllChannelIds() {
        return PredefinedChannels.MAP.keySet();
    }

    /**
     * @return A set of channel ids of channels that should be initialized on startup.
     */
    static Set<String> getStartupChannelIds() {
        // CHANNELS_VERSION must be incremented if the set of channels returned here changes.
        return PredefinedChannels.STARTUP;
    }

    /**
     * @return A set of channel group ids of channel groups that should be initialized on startup.
     */
    static Set<String> getStartupChannelGroupIds() {
        Set<String> groupIds = new HashSet<>();
        for (String id : getStartupChannelIds()) {
            groupIds.add(getChannelFromId(id).mGroupId);
        }
        return groupIds;
    }

    /**
     * @return An array of old ChannelIds that may have been returned by
     *         {@link #getStartupChannelIds} in the past, but are no longer in use.
     */
    static List<String> getLegacyChannelIds() {
        List<String> legacyChannels = new ArrayList<>(Arrays.asList(LEGACY_CHANNEL_IDS));
        return legacyChannels;
    }

    static PredefinedChannelGroup getChannelGroupForChannel(PredefinedChannel channel) {
        return getChannelGroup(channel.mGroupId);
    }

    static PredefinedChannelGroup getChannelGroup(@ChannelGroupId String groupId) {
        return PredefinedChannelGroups.MAP.get(groupId);
    }

    static PredefinedChannel getChannelFromId(@ChannelId String channelId) {
        return PredefinedChannels.MAP.get(channelId);
    }

    /**
     * Helper class for storing predefined channel properties while allowing the channel name to be
     * lazily evaluated only when it is converted to an actual NotificationChannel.
     */
    static class PredefinedChannel {
        @ChannelId
        private final String mId;
        private final int mNameResId;
        private final int mImportance;
        @ChannelGroupId
        private final String mGroupId;
        private final boolean mShowNotificationBadges;

        PredefinedChannel(@ChannelId String id, int nameResId, int importance,
                @ChannelGroupId String groupId) {
            this(id, nameResId, importance, groupId, false /* showNotificationBadges */);
        }

        PredefinedChannel(@ChannelId String id, int nameResId, int importance,
                @ChannelGroupId String groupId, boolean showNotificationBadges) {
            this.mId = id;
            this.mNameResId = nameResId;
            this.mImportance = importance;
            this.mGroupId = groupId;
            this.mShowNotificationBadges = showNotificationBadges;
        }

        NotificationChannel toNotificationChannel(Resources resources) {
            String name = resources.getString(mNameResId);
            NotificationChannel channel = new NotificationChannel(mId, name, mImportance);
            channel.setGroup(mGroupId);
            channel.setShowBadge(mShowNotificationBadges);
            return channel;
        }
    }

    /**
     * Helper class for storing predefined channel group properties while allowing the group name to
     * be lazily evaluated only when it is converted to an actual NotificationChannelGroup.
     */
    public static class PredefinedChannelGroup {
        @ChannelGroupId
        public final String mId;
        public final int mNameResId;

        PredefinedChannelGroup(@ChannelGroupId String id, int nameResId) {
            this.mId = id;
            this.mNameResId = nameResId;
        }

        NotificationChannelGroup toNotificationChannelGroup(Resources resources) {
            String name = resources.getString(mNameResId);
            return new NotificationChannelGroup(mId, name);
        }
    }
}
