// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.NotificationCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.widget.RemoteViews;

import org.chromium.base.Log;
import org.chromium.chrome.browser.notifications.channels.ChannelsInitializer;

/**
 * Wraps a NotificationCompat.Builder object.
 */
public class NotificationCompatBuilder implements ChromeNotificationBuilder {
    private static final String TAG = "NotifCompatBuilder";
    private final NotificationCompat.Builder mBuilder;
    private final NotificationMetadata mMetadata;

    NotificationCompatBuilder(Context context, String channelId,
            ChannelsInitializer channelsInitializer, NotificationMetadata metadata) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            channelsInitializer.safeInitialize(channelId);
        }
        mBuilder = new NotificationCompat.Builder(context, channelId);
        mMetadata = metadata;
        if (mMetadata != null) {
            mBuilder.setDeleteIntent(
                    NotificationIntentInterceptor.getDefaultDeletePendingIntent(mMetadata));
        }
    }

    @Override
    public ChromeNotificationBuilder setAutoCancel(boolean autoCancel) {
        mBuilder.setAutoCancel(autoCancel);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentIntent(PendingIntent contentIntent) {
        mBuilder.setContentIntent(contentIntent);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentIntent(PendingIntentProvider contentIntent) {
        assert mMetadata != null;
        PendingIntent pendingIntent = NotificationIntentInterceptor.createInterceptPendingIntent(
                NotificationIntentInterceptor.IntentType.CONTENT_INTENT, 0 /* intentId */,
                mMetadata, contentIntent);
        mBuilder.setContentIntent(pendingIntent);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentTitle(CharSequence title) {
        mBuilder.setContentTitle(title);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentText(CharSequence text) {
        mBuilder.setContentText(text);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setSmallIcon(int icon) {
        mBuilder.setSmallIcon(icon);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setSmallIcon(Icon icon) {
        assert false; // unused
        return this;
    }

    @Override
    public ChromeNotificationBuilder setColor(int argb) {
        mBuilder.setColor(argb);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setTicker(CharSequence text) {
        mBuilder.setTicker(text);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setLocalOnly(boolean localOnly) {
        mBuilder.setLocalOnly(localOnly);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setGroup(String group) {
        mBuilder.setGroup(group);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setGroupSummary(boolean isGroupSummary) {
        mBuilder.setGroupSummary(isGroupSummary);
        return this;
    }

    @Override
    public ChromeNotificationBuilder addExtras(Bundle extras) {
        mBuilder.addExtras(extras);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setOngoing(boolean ongoing) {
        mBuilder.setOngoing(ongoing);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setVisibility(int visibility) {
        mBuilder.setVisibility(visibility);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setShowWhen(boolean showWhen) {
        mBuilder.setShowWhen(showWhen);
        return this;
    }

    @Override
    public ChromeNotificationBuilder addAction(int icon, CharSequence title, PendingIntent intent) {
        mBuilder.addAction(icon, title, intent);
        return this;
    }

    @Override
    public ChromeNotificationBuilder addAction(int icon, CharSequence title,
            PendingIntentProvider pendingIntentProvider,
            @NotificationUmaTracker.ActionType int actionType) {
        assert (mMetadata != null);
        PendingIntent pendingIntent = NotificationIntentInterceptor.createInterceptPendingIntent(
                NotificationIntentInterceptor.IntentType.ACTION_INTENT, actionType, mMetadata,
                pendingIntentProvider);
        addAction(icon, title, pendingIntent);
        return this;
    }

    @Override
    public ChromeNotificationBuilder addAction(Notification.Action action) {
        return this;
    }

    @Override
    public ChromeNotificationBuilder addAction(Notification.Action action, int flags,
            @NotificationUmaTracker.ActionType int actionType) {
        return this;
    }

    @Override
    public ChromeNotificationBuilder setDeleteIntent(PendingIntent intent) {
        mBuilder.setDeleteIntent(intent);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setDeleteIntent(PendingIntentProvider intent) {
        assert (mMetadata != null);
        mBuilder.setDeleteIntent(NotificationIntentInterceptor.createInterceptPendingIntent(
                NotificationIntentInterceptor.IntentType.DELETE_INTENT, 0 /* intentId */, mMetadata,
                intent));
        return this;
    }

    @Override
    public ChromeNotificationBuilder setPriorityBeforeO(int pri) {
        mBuilder.setPriority(pri);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setProgress(int max, int percentage, boolean indeterminate) {
        mBuilder.setProgress(max, percentage, indeterminate);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setSubText(CharSequence text) {
        mBuilder.setSubText(text);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContentInfo(String info) {
        mBuilder.setContentInfo(info);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setWhen(long time) {
        mBuilder.setWhen(time);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setLargeIcon(Bitmap icon) {
        mBuilder.setLargeIcon(icon);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setVibrate(long[] vibratePattern) {
        mBuilder.setVibrate(vibratePattern);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setDefaults(int defaults) {
        mBuilder.setDefaults(defaults);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setOnlyAlertOnce(boolean onlyAlertOnce) {
        mBuilder.setOnlyAlertOnce(onlyAlertOnce);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setPublicVersion(Notification publicNotification) {
        mBuilder.setPublicVersion(publicNotification);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setContent(RemoteViews views) {
        mBuilder.setCustomContentView(views);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setStyle(Notification.BigPictureStyle style) {
        assert false; // unused
        return this;
    }

    @Override
    public ChromeNotificationBuilder setStyle(Notification.BigTextStyle bigTextStyle) {
        assert false; // unused
        return this;
    }

    @Override
    public ChromeNotificationBuilder setMediaStyle(MediaSessionCompat session, int[] actions,
            PendingIntent intent, boolean showCancelButton) {
        android.support.v4.media.app.NotificationCompat.MediaStyle style =
                new android.support.v4.media.app.NotificationCompat.MediaStyle();
        style.setMediaSession(session.getSessionToken());
        style.setShowActionsInCompactView(actions);
        style.setCancelButtonIntent(intent);
        style.setShowCancelButton(showCancelButton);
        mBuilder.setStyle(style);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setCategory(String category) {
        mBuilder.setCategory(category);
        return this;
    }

    @Override
    public ChromeNotification buildWithBigContentView(RemoteViews view) {
        assert mMetadata != null;
        return new ChromeNotification(mBuilder.setCustomBigContentView(view).build(), mMetadata);
    }

    @Override
    public ChromeNotification buildWithBigTextStyle(String bigText) {
        NotificationCompat.BigTextStyle bigTextStyle =
                new NotificationCompat.BigTextStyle(mBuilder);
        bigTextStyle.bigText(bigText);

        assert mMetadata != null;
        return new ChromeNotification(bigTextStyle.build(), mMetadata);
    }

    @Override
    public Notification build() {
        Notification notification = null;
        try {
            notification = mBuilder.build();
        } catch (NullPointerException e) {
            // Android M and L may throw exception, see https://crbug.com/949794.
            Log.e(TAG, "Failed to build notification.", e);
            if (mMetadata != null) {
                NotificationUmaTracker.onNotificationFailedToCreate(mMetadata.type);
            }
        }
        return notification;
    }

    @Override
    public ChromeNotification buildChromeNotification() {
        assert mMetadata != null;
        return new ChromeNotification(build(), mMetadata);
    }
}
