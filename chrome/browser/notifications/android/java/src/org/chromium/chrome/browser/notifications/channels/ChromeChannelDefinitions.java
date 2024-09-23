// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;
import androidx.annotation.StringDef;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.notifications.R;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelDefinitions;

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
 * Contains Chrome-specific properties for pre-definable notification channels on Android O+. In
 * practice this is all our channels except site channels, which are defined dynamically by the
 * {@link SiteChannelsManager}. <br>
 * <br>
 * PLEASE NOTE: Notification channels appear in system UI and their properties are persisted forever
 * by Android, so should not be added or removed lightly, and the proper deprecation and versioning
 * steps must be taken when doing so. <br>
 * <br>
 * See the README.md in this directory for more information before adding or changing any channels.
 */
@RequiresApi(Build.VERSION_CODES.O)
public class ChromeChannelDefinitions extends ChannelDefinitions {
    /**
     * Version number identifying the current set of channels. This must be incremented whenever the
     * set of channels returned by {@link #getStartupChannelIds()} or {@link #getLegacyChannelIds()}
     * changes.
     */
    static final int CHANNELS_VERSION = 4;

    private static class LazyHolder {
        private static ChromeChannelDefinitions sInstance = new ChromeChannelDefinitions();
    }

    public static ChromeChannelDefinitions getInstance() {
        return LazyHolder.sInstance;
    }

    private ChromeChannelDefinitions() {}

    /**
     * Keeps the value consistent with {@link
     * org.chromium.webapk.shell_apk.WebApkServiceImplWrapper#DEFAULT_NOTIFICATION_CHANNEL_ID}.
     */
    public static final String CHANNEL_ID_WEBAPKS = "default_channel_id";

    /**
     * To define a new channel, add the channel ID to this StringDef and add a new entry to
     * PredefinedChannels.MAP below with the appropriate channel parameters. To remove an existing
     * channel, remove the ID from this StringDef, remove its entry from Predefined Channels.MAP,
     * and add the ID to the LEGACY_CHANNELS_ID array below. See the README in this directory for
     * more detailed instructions.
     */
    @StringDef({
        ChannelId.BROWSER,
        ChannelId.DOWNLOADS,
        ChannelId.INCOGNITO,
        ChannelId.MEDIA_PLAYBACK,
        ChannelId.SCREEN_CAPTURE,
        ChannelId.CONTENT_SUGGESTIONS,
        ChannelId.WEBAPP_ACTIONS,
        ChannelId.SITES,
        ChannelId.VR,
        ChannelId.SHARING,
        ChannelId.UPDATES,
        ChannelId.COMPLETED_DOWNLOADS,
        ChannelId.PERMISSION_REQUESTS,
        ChannelId.PERMISSION_REQUESTS_HIGH,
        ChannelId.ANNOUNCEMENT,
        ChannelId.WEBAPPS,
        ChannelId.WEBAPPS_QUIET,
        ChannelId.WEBRTC_CAM_AND_MIC,
        ChannelId.PRICE_DROP,
        ChannelId.PRICE_DROP_DEFAULT,
        ChannelId.SECURITY_KEY,
        ChannelId.BLUETOOTH,
        ChannelId.USB
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChannelId {
        String BROWSER = "browser";
        String DOWNLOADS = "downloads";
        String INCOGNITO = "incognito";
        String MEDIA_PLAYBACK = "media";
        String SCREEN_CAPTURE = "screen_capture";
        String CONTENT_SUGGESTIONS = "content_suggestions";
        String WEBAPP_ACTIONS = "webapp_actions";
        // TODO(crbug.com/40510194): Deprecate the 'sites' channel.
        String SITES = "sites";
        String VR = "vr";
        String SHARING = "sharing";
        String UPDATES = "updates";
        String COMPLETED_DOWNLOADS = "completed_downloads";
        String PERMISSION_REQUESTS = "permission_requests";
        String PERMISSION_REQUESTS_HIGH = "permission_requests_high";
        String ANNOUNCEMENT = "announcement";
        String WEBAPPS = "twa_disclosure_initial";
        String WEBAPPS_QUIET = "twa_disclosure_subsequent";
        String WEBRTC_CAM_AND_MIC = "webrtc_cam_and_mic";
        // TODO(crbug.com/40244973): This PRICE_DROP channel is initialized with IMPORTANCE_LOW in
        // M107. To update the initial importance level to DEFAULT, we have to introduce a new
        // channel PRICE_DROP_DEFAULT and deprecate this one. Since we want to initialize the new
        // channel based on this old channel's status to keep users' experience consistent, this old
        // channel will be kept for one or two milestones and then be cleaned up.
        String PRICE_DROP = "shopping_price_drop_alerts";
        String PRICE_DROP_DEFAULT = "shopping_price_drop_alerts_default";
        String SECURITY_KEY = "security_key";
        String BLUETOOTH = "bluetooth";
        String USB = "usb";
    }

    @StringDef({ChannelGroupId.GENERAL, ChannelGroupId.SITES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChannelGroupId {
        String SITES = "sites";
        String GENERAL = "general";
    }

    // Map defined in static inner class so it's only initialized lazily.
    private static class PredefinedChannels {
        /**
         * Definitions for predefined channels. Any channel listed in STARTUP must have an entry in
         * this map; it may also contain channels that are enabled conditionally.
         */
        static final Map<String, PredefinedChannel> MAP;

        /**
         * The set of predefined channels to be initialized on startup.
         *
         * <p>CHANNELS_VERSION must be incremented every time an entry is added to or removed from
         * this set, or when the definition of one of a channel in this set is changed. If an entry
         * is removed from here then it must be added to the LEGACY_CHANNEL_IDs array.
         */
        static final Set<String> STARTUP;

        static {
            Map<String, PredefinedChannel> map = new HashMap<>();
            Set<String> startup = new HashSet<>();

            map.put(
                    ChannelId.BROWSER,
                    PredefinedChannel.create(
                            ChannelId.BROWSER,
                            R.string.notification_category_browser,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));
            startup.add(ChannelId.BROWSER);

            map.put(
                    ChannelId.DOWNLOADS,
                    PredefinedChannel.create(
                            ChannelId.DOWNLOADS,
                            R.string.notification_category_downloads,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));
            startup.add(ChannelId.DOWNLOADS);

            map.put(
                    ChannelId.INCOGNITO,
                    PredefinedChannel.create(
                            ChannelId.INCOGNITO,
                            R.string.notification_category_incognito,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));
            startup.add(ChannelId.INCOGNITO);

            map.put(
                    ChannelId.MEDIA_PLAYBACK,
                    PredefinedChannel.create(
                            ChannelId.MEDIA_PLAYBACK,
                            R.string.notification_category_media_playback,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));
            startup.add(ChannelId.MEDIA_PLAYBACK);

            map.put(
                    ChannelId.WEBRTC_CAM_AND_MIC,
                    PredefinedChannel.create(
                            ChannelId.WEBRTC_CAM_AND_MIC,
                            R.string.notification_category_webrtc_cam_and_mic,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));

            // ChannelId.SCREEN_CAPTURE will be created on first use, instead of on startup,
            // so that it doesn't clutter the list for users who don't use this feature.
            map.put(
                    ChannelId.SCREEN_CAPTURE,
                    PredefinedChannel.create(
                            ChannelId.SCREEN_CAPTURE,
                            R.string.notification_category_screen_capture,
                            NotificationManager.IMPORTANCE_HIGH,
                            ChannelGroupId.GENERAL));

            map.put(
                    ChannelId.SHARING,
                    PredefinedChannel.create(
                            ChannelId.SHARING,
                            R.string.notification_category_sharing,
                            NotificationManager.IMPORTANCE_HIGH,
                            ChannelGroupId.GENERAL));
            // Not adding sites channel to startup channels because notifications may be posted to
            // this channel if no site-specific channel could be found.
            // TODO(crbug.com/40558363) Stop using this channel as a fallback and fully deprecate
            // it.
            map.put(
                    ChannelId.SITES,
                    PredefinedChannel.create(
                            ChannelId.SITES,
                            R.string.notification_category_sites,
                            NotificationManager.IMPORTANCE_DEFAULT,
                            ChannelGroupId.GENERAL));

            // Not adding to startup channels because this channel is experimental and enabled only
            // through the associated feature (see
            // org.chromium.chrome.browser.ntp.ContentSuggestionsNotificationHelper).
            map.put(
                    ChannelId.CONTENT_SUGGESTIONS,
                    PredefinedChannel.create(
                            ChannelId.CONTENT_SUGGESTIONS,
                            R.string.notification_category_content_suggestions,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));

            // Not adding to startup channels because we want ChannelId.WEBAPP_ACTIONS to be
            // created on the first use, as not all users use installed web apps.
            map.put(
                    ChannelId.WEBAPP_ACTIONS,
                    PredefinedChannel.create(
                            ChannelId.WEBAPP_ACTIONS,
                            R.string.notification_category_fullscreen_controls,
                            NotificationManager.IMPORTANCE_MIN,
                            ChannelGroupId.GENERAL));

            // Not adding to startup channels because we want ChannelId.VR to be created on the
            // first use, as not all users use VR. Channel must have high importance for
            // notifications to show up in VR.
            map.put(
                    ChannelId.VR,
                    PredefinedChannel.create(
                            ChannelId.VR,
                            R.string.notification_category_vr,
                            NotificationManager.IMPORTANCE_HIGH,
                            ChannelGroupId.GENERAL));

            map.put(
                    ChannelId.UPDATES,
                    PredefinedChannel.create(
                            ChannelId.UPDATES,
                            R.string.notification_category_updates,
                            NotificationManager.IMPORTANCE_HIGH,
                            ChannelGroupId.GENERAL));

            map.put(
                    ChannelId.COMPLETED_DOWNLOADS,
                    PredefinedChannel.createBadged(
                            ChannelId.COMPLETED_DOWNLOADS,
                            R.string.notification_category_completed_downloads,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));

            map.put(
                    ChannelId.ANNOUNCEMENT,
                    PredefinedChannel.createBadged(
                            ChannelId.ANNOUNCEMENT,
                            R.string.notification_category_announcement,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));

            // Not added to startup channels as not all users will use Trusted Web Activities.
            map.put(
                    ChannelId.WEBAPPS,
                    PredefinedChannel.createSilenced(
                            ChannelId.WEBAPPS,
                            R.string.notification_category_webapps,
                            NotificationManager.IMPORTANCE_MAX,
                            ChannelGroupId.GENERAL));
            map.put(
                    ChannelId.WEBAPPS_QUIET,
                    PredefinedChannel.create(
                            ChannelId.WEBAPPS_QUIET,
                            R.string.notification_category_webapps_quiet,
                            NotificationManager.IMPORTANCE_MIN,
                            ChannelGroupId.GENERAL));

            // Not added to startup channels because we want this channel to be created on the first
            // use.
            map.put(
                    ChannelId.PRICE_DROP,
                    PredefinedChannel.create(
                            ChannelId.PRICE_DROP,
                            R.string.notification_category_price_drop,
                            NotificationManager.IMPORTANCE_DEFAULT,
                            ChannelGroupId.GENERAL));
            // TODO(crbug.com/40244973): Make the new channel's behavior consistent with the old
            // channel's if it's created and modified by the user. Clean this up after one or two
            // milestones.
            int priceDropDefaultChannelImportance = NotificationManager.IMPORTANCE_DEFAULT;
            if (VERSION.SDK_INT >= VERSION_CODES.O) {
                NotificationManagerProxy notificationManager =
                        new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
                NotificationChannel priceDropChannel =
                        notificationManager.getNotificationChannel(ChannelId.PRICE_DROP);
                if (priceDropChannel != null) {
                    startup.add(ChannelId.PRICE_DROP_DEFAULT);
                    if (priceDropChannel.getImportance() != NotificationManager.IMPORTANCE_LOW) {
                        priceDropDefaultChannelImportance = priceDropChannel.getImportance();
                    }
                    notificationManager.deleteNotificationChannel(ChannelId.PRICE_DROP);
                }
            }
            map.put(
                    ChannelId.PRICE_DROP_DEFAULT,
                    PredefinedChannel.create(
                            ChannelId.PRICE_DROP_DEFAULT,
                            R.string.notification_category_price_drop,
                            priceDropDefaultChannelImportance,
                            ChannelGroupId.GENERAL));

            // The security key notification channel will only appear for users
            // who use this feature.
            map.put(
                    ChannelId.SECURITY_KEY,
                    PredefinedChannel.create(
                            ChannelId.SECURITY_KEY,
                            R.string.notification_category_security_key,
                            NotificationManager.IMPORTANCE_HIGH,
                            ChannelGroupId.GENERAL));

            // The bluetooth notification channel will only appear for users
            // who are targeted for this feature.
            map.put(
                    ChannelId.BLUETOOTH,
                    PredefinedChannel.create(
                            ChannelId.BLUETOOTH,
                            R.string.notification_category_bluetooth,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));

            // The usb notification channel will only appear for users
            // who are targeted for this feature.
            map.put(
                    ChannelId.USB,
                    PredefinedChannel.create(
                            ChannelId.USB,
                            R.string.notification_category_usb,
                            NotificationManager.IMPORTANCE_LOW,
                            ChannelGroupId.GENERAL));

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
        ChromeChannelDefinitions.ChannelId.SITES,
        ChromeChannelDefinitions.ChannelId.PERMISSION_REQUESTS,
        ChromeChannelDefinitions.ChannelId.PERMISSION_REQUESTS_HIGH,
    };

    // Map defined in static inner class so it's only initialized lazily.
    private static class PredefinedChannelGroups {
        static final Map<String, PredefinedChannelGroup> MAP;

        static {
            Map<String, PredefinedChannelGroup> map = new HashMap<>();
            map.put(
                    ChannelGroupId.GENERAL,
                    new PredefinedChannelGroup(
                            ChannelGroupId.GENERAL, R.string.notification_category_group_general));
            map.put(
                    ChannelGroupId.SITES,
                    new PredefinedChannelGroup(
                            ChannelGroupId.SITES, R.string.notification_category_group_sites));
            MAP = Collections.unmodifiableMap(map);
        }
    }

    @Override
    public Set<String> getAllChannelGroupIds() {
        return PredefinedChannelGroups.MAP.keySet();
    }

    @Override
    public Set<String> getAllChannelIds() {
        return PredefinedChannels.MAP.keySet();
    }

    @Override
    public Set<String> getStartupChannelIds() {
        // CHANNELS_VERSION must be incremented if the set of channels returned here changes.
        return PredefinedChannels.STARTUP;
    }

    @Override
    public List<String> getLegacyChannelIds() {
        List<String> legacyChannels = new ArrayList<>(Arrays.asList(LEGACY_CHANNEL_IDS));
        return legacyChannels;
    }

    @Override
    public PredefinedChannelGroup getChannelGroup(@ChannelGroupId String groupId) {
        return PredefinedChannelGroups.MAP.get(groupId);
    }

    @Override
    public PredefinedChannel getChannelFromId(@ChannelId String channelId) {
        return PredefinedChannels.MAP.get(channelId);
    }

    @Override
    public boolean isValidNonPredefinedChannelId(String channelId) {
        return SiteChannelsManager.isValidSiteChannelId(channelId)
                || TextUtils.equals(channelId, ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS);
    }
}
