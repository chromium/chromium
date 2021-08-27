// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/**
 * Factory which supplies the appropriate type of notification builder based on Android version.
 * Should be used for all notifications we create, to ensure a notification channel is set on O.
 */
public class NotificationWrapperBuilderFactory {
    /**
     * Creates either a Notification.Builder or NotificationCompat.Builder under the hood, wrapped
     * in our own common interface, and ensures the notification channel has been initialized.
     *
     * @param preferCompat true if a NotificationCompat.Builder is preferred. You should pick true
     *                     unless you know NotificationCompat.Builder doesn't support a feature you
     *                     require.
     * @param channelId The ID of the channel the notification should be posted to. This channel
     *                  will be created if it did not already exist. Must be a known channel within
     *                  {@link ChannelsInitializer#ensureInitialized(String)}.
     *
     * @deprecated Use {@link #createNotificationWrapperBuilder(String)} instead
     */
    @Deprecated
    public static NotificationWrapperBuilder createNotificationWrapperBuilder(
            boolean preferCompat, String channelId) {
        return createNotificationWrapperBuilder(
                preferCompat, channelId, null /* remoteAppPackageName */, null /* metadata */);
    }

    /**
     * See {@link #createNotificationWrapperBuilder(boolean, String, String, NotificationMetadata)}.
     *
     * @deprecated Use {@link #createNotificationWrapperBuilder(String)} instead
     */
    @Deprecated
    public static NotificationWrapperBuilder createNotificationWrapperBuilder(
            boolean preferCompat, String channelId, @Nullable String remoteAppPackageName) {
        return createNotificationWrapperBuilder(
                preferCompat, channelId, remoteAppPackageName, null /* metadata */);
    }

    /**
     * Same as above, with additional parameter:
     * @param remoteAppPackageName if not null, tries to create a Context from the package name
     * and passes it to the builder.
     * @param metadata Metadata contains notification id, tag, etc.
     *
     * @deprecated Use {@link #createNotificationWrapperBuilder(String, NotificationMetadata)}
     *         instead
     */
    @Deprecated
    public static NotificationWrapperBuilder createNotificationWrapperBuilder(boolean preferCompat,
            String channelId, @Nullable String remoteAppPackageName,
            @Nullable NotificationMetadata metadata) {
        assert preferCompat;
        assert remoteAppPackageName == null;
        return createNotificationWrapperBuilder(channelId, metadata);
    }

    /**
     * Creates either a NotificationCompat.Builder under the hood, wrapped in our own interface, and
     * ensures the notification channel has been initialized.
     *
     * @param channelId The ID of the channel the notification should be posted to. This channel
     *                  will be created if it did not already exist. Must be a known channel within
     *                  {@link ChannelsInitializer#ensureInitialized(String)}.
     */
    public static NotificationWrapperBuilder createNotificationWrapperBuilder(String channelId) {
        return createNotificationWrapperBuilder(channelId, null /* metadata */);
    }

    /**
     * Same as above, with additional parameter:
     * @param metadata Metadata contains notification id, tag, etc.
     */
    public static NotificationWrapperBuilder createNotificationWrapperBuilder(
            String channelId, @Nullable NotificationMetadata metadata) {
        Context context = ContextUtils.getApplicationContext();

        NotificationManagerProxyImpl notificationManagerProxy =
                new NotificationManagerProxyImpl(context);

        ChannelsInitializer channelsInitializer = new ChannelsInitializer(notificationManagerProxy,
                ChromeChannelDefinitions.getInstance(), context.getResources());

        return new ChromeNotificationWrapperCompatBuilder(
                context, channelId, channelsInitializer, metadata);
    }
}
