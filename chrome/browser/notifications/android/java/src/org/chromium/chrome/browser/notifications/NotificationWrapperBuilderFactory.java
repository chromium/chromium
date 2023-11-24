// Copyright 2021 The Chromium Authors
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
     * Creates a NotificationCompat.Builder under the hood, wrapped in our own interface, and
     * ensures the notification channel has been initialized.
     *
     * @param channelId The ID of the channel the notification should be posted to. This channel
     *     will be created if it did not already exist. Must be a known channel within {@link
     *     ChannelsInitializer#ensureInitialized(String)}.
     */
    public static NotificationWrapperBuilder createNotificationWrapperBuilder(String channelId) {
        return createNotificationWrapperBuilder(channelId, /* metadata= */ null);
    }

    /**
     * Same as above, with additional parameter:
     *
     * @param metadata Metadata contains notification id, tag, etc.
     */
    public static NotificationWrapperBuilder createNotificationWrapperBuilder(
            String channelId, @Nullable NotificationMetadata metadata) {
        Context context = ContextUtils.getApplicationContext();

        NotificationManagerProxyImpl notificationManagerProxy =
                new NotificationManagerProxyImpl(context);

        ChannelsInitializer channelsInitializer =
                new ChannelsInitializer(
                        notificationManagerProxy,
                        ChromeChannelDefinitions.getInstance(),
                        context.getResources());

        return new ChromeNotificationWrapperCompatBuilder(
                context, channelId, channelsInitializer, metadata);
    }
}
