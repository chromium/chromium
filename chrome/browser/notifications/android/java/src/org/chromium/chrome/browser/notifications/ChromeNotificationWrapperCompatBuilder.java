// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.PendingIntent;
import android.content.Context;

import androidx.core.app.NotificationCompat;

import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.NotificationWrapperCompatBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/**
 * Extends the base NotificationWrapperCompatBuilder to add UMA by way of {@link
 * NotificationIntentInterceptor}.
 */
public class ChromeNotificationWrapperCompatBuilder extends NotificationWrapperCompatBuilder {
    ChromeNotificationWrapperCompatBuilder(
            Context context,
            String channelId,
            ChannelsInitializer channelsInitializer,
            NotificationMetadata metadata) {
        super(context, channelId, channelsInitializer, metadata);
        if (metadata != null) {
            getBuilder()
                    .setDeleteIntent(
                            NotificationIntentInterceptor.getDefaultDeletePendingIntent(metadata));
        }
    }

    @Override
    public NotificationWrapperBuilder setContentIntent(PendingIntentProvider contentIntent) {
        PendingIntent pendingIntent =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        /* actionType= */ NotificationUmaTracker.ActionType.UNKNOWN,
                        getMetadata(),
                        contentIntent);
        return setContentIntent(pendingIntent);
    }

    @Override
    public NotificationWrapperBuilder addAction(
            int icon,
            CharSequence title,
            PendingIntentProvider pendingIntentProvider,
            @NotificationUmaTracker.ActionType int actionType) {
        PendingIntent pendingIntent =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                        actionType,
                        getMetadata(),
                        pendingIntentProvider);
        return addAction(icon, title, pendingIntent);
    }

    @Override
    public NotificationWrapperBuilder addAction(
            NotificationCompat.Action action,
            int flags,
            @NotificationUmaTracker.ActionType int actionType,
            int requestCode) {
        PendingIntent pendingIntent =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                        actionType,
                        getMetadata(),
                        new PendingIntentProvider(action.actionIntent, flags, requestCode));
        action.actionIntent = pendingIntent;
        return addAction(action);
    }

    @Override
    public NotificationWrapperBuilder setDeleteIntent(PendingIntentProvider intent) {
        return setDeleteIntent(
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.DELETE_INTENT,
                        /* actionType= */ NotificationUmaTracker.ActionType.UNKNOWN,
                        getMetadata(),
                        intent));
    }

    @Override
    public NotificationWrapperBuilder setDeleteIntent(
            PendingIntentProvider intent, @NotificationUmaTracker.ActionType int actionType) {
        // As `actionType` will be part of the `requestCode` that `NotificationIntentInterceptor`
        // generates, the below wrapper `PendingIntent` will not be `Intent.filterEquals` to the
        // one above.
        return setDeleteIntent(
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                        actionType,
                        getMetadata(),
                        intent));
    }
}
