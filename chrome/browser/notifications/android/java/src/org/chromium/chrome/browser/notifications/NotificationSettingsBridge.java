// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.NotificationChannel;
import android.app.NotificationManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;

/** Interface for native code to interact with Android notification channels. */
public class NotificationSettingsBridge {
    /**
     * Creates a notification channel for the given origin, unless a channel for this origin already
     * exists.
     *
     * @param origin The site origin to be used as the channel name.
     * @param creationTime A string representing the time of channel creation.
     * @param enabled True if the channel should be initially enabled, false if it should start off
     *     as blocked.
     * @return The channel created for this origin.
     */
    @CalledByNative
    static SiteChannel createChannel(String origin, long creationTime, boolean enabled) {
        return SiteChannelsManager.getInstance().createSiteChannel(origin, creationTime, enabled);
    }

    @CalledByNative
    private static void getSiteChannels(final long callbackId) {
        SiteChannel[] channels = SiteChannelsManager.getInstance().getSiteChannels();
        NotificationSettingsBridgeJni.get().onGetSiteChannelsDone(callbackId, channels);
    }

    @CalledByNative
    static void deleteChannel(String channelId) {
        SiteChannelsManager.getInstance().deleteSiteChannel(channelId);
    }

    /** Helper type for passing site channel objects across the JNI. */
    public static class SiteChannel {
        private final String mId;
        private final String mOrigin;
        private final long mTimestamp;
        private final @NotificationChannelStatus int mStatus;

        public SiteChannel(
                String channelId,
                String origin,
                long creationTimestamp,
                @NotificationChannelStatus int status) {
            mId = channelId;
            mOrigin = origin;
            mTimestamp = creationTimestamp;
            mStatus = status;
        }

        @CalledByNative("SiteChannel")
        public long getTimestamp() {
            return mTimestamp;
        }

        @CalledByNative("SiteChannel")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("SiteChannel")
        public @NotificationChannelStatus int getStatus() {
            return mStatus;
        }

        @CalledByNative("SiteChannel")
        public String getId() {
            return mId;
        }

        public NotificationChannel toChannel() {
            NotificationChannel channel =
                    new NotificationChannel(
                            mId,
                            UrlFormatter.formatUrlForSecurityDisplay(
                                    mOrigin, SchemeDisplay.OMIT_HTTP_AND_HTTPS),
                            mStatus == NotificationChannelStatus.BLOCKED
                                    ? NotificationManager.IMPORTANCE_NONE
                                    : NotificationManager.IMPORTANCE_DEFAULT);
            channel.setGroup(ChromeChannelDefinitions.ChannelGroupId.SITES);
            return channel;
        }
    }

    @NativeMethods
    interface Natives {
        void onGetSiteChannelsDone(long callbackId, SiteChannel[] channels);
    }
}
