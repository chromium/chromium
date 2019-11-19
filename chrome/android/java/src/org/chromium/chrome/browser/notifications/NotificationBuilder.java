// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.media.session.MediaSession;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.media.session.MediaSessionCompat;
import android.widget.RemoteViews;

import org.chromium.chrome.browser.notifications.channels.ChannelsInitializer;

/**
 * Wraps a Notification.Builder object.
 */
public class NotificationBuilder implements ChromeNotificationBuilder {
    private final Notification.Builder mBuilder;
    private final Context mContext;
    private final NotificationMetadata mMetadata;

    NotificationBuilder(Context context, String channelId, ChannelsInitializer channelsInitializer,
            NotificationMetadata metadata) {
        mContext = context;
        mBuilder = new Notification.Builder(mContext);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            channelsInitializer.safeInitialize(channelId);
            mBuilder.setChannelId(channelId);
        }
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
        assert (mMetadata != null);
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mBuilder.setSmallIcon(icon);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setColor(int argb) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            mBuilder.setColor(argb);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setTicker(CharSequence text) {
        mBuilder.setTicker(text);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setLocalOnly(boolean localOnly) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            mBuilder.setLocalOnly(localOnly);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setGroup(String group) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            mBuilder.setGroup(group);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setGroupSummary(boolean isGroupSummary) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            mBuilder.setGroupSummary(isGroupSummary);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder addExtras(Bundle extras) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            mBuilder.addExtras(extras);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setOngoing(boolean ongoing) {
        mBuilder.setOngoing(ongoing);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setVisibility(int visibility) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            mBuilder.setVisibility(visibility);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setShowWhen(boolean showWhen) {
        mBuilder.setShowWhen(showWhen);
        return this;
    }

    @Override
    @SuppressWarnings("deprecation")
    public ChromeNotificationBuilder addAction(int icon, CharSequence title, PendingIntent intent) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mBuilder.addAction(
                    new Notification.Action
                            .Builder(Icon.createWithResource(mContext, icon), title, intent)
                            .build());
        } else {
            mBuilder.addAction(icon, title, intent);
        }
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            mBuilder.addAction(action);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder addAction(Notification.Action action, int flags,
            @NotificationUmaTracker.ActionType int actionType) {
        assert (mMetadata != null);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            PendingIntent pendingIntent =
                    NotificationIntentInterceptor.createInterceptPendingIntent(
                            NotificationIntentInterceptor.IntentType.ACTION_INTENT, actionType,
                            mMetadata, new PendingIntentProvider(action.actionIntent, flags));
            action.actionIntent = pendingIntent;
            addAction(action);
        }
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
    @SuppressWarnings("deprecation")
    public ChromeNotificationBuilder setPriorityBeforeO(int pri) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            mBuilder.setPriority(pri);
        }
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
    @SuppressWarnings("deprecation")
    public ChromeNotificationBuilder setContentInfo(String info) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            mBuilder.setContentInfo(info);
        } else {
            mBuilder.setSubText(info);
        }
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            mBuilder.setPublicVersion(publicNotification);
        }
        return this;
    }

    @Override
    @SuppressWarnings("deprecation")
    public ChromeNotificationBuilder setContent(RemoteViews views) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            mBuilder.setCustomContentView(views);
        } else {
            mBuilder.setContent(views);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setStyle(Notification.BigPictureStyle style) {
        mBuilder.setStyle(style);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setStyle(Notification.BigTextStyle style) {
        mBuilder.setStyle(style);
        return this;
    }

    @Override
    public ChromeNotificationBuilder setMediaStyle(MediaSessionCompat session, int[] actions,
            PendingIntent intent, boolean showCancelButton) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Notification.MediaStyle style = new Notification.MediaStyle();
            style.setMediaSession(((MediaSession) session.getMediaSession()).getSessionToken());
            style.setShowActionsInCompactView(actions);
            mBuilder.setStyle(style);
        }
        return this;
    }

    @Override
    public ChromeNotificationBuilder setCategory(String category) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            mBuilder.setCategory(category);
        }
        return this;
    }

    @Override
    @SuppressWarnings("deprecation")
    public ChromeNotification buildWithBigContentView(RemoteViews view) {
        assert mMetadata != null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return new ChromeNotification(
                    mBuilder.setCustomBigContentView(view).build(), mMetadata);
        } else {
            Notification notification = mBuilder.build();
            notification.bigContentView = view;
            return new ChromeNotification(notification, mMetadata);
        }
    }

    @Override
    public ChromeNotification buildWithBigTextStyle(String bigText) {
        Notification.BigTextStyle bigTextStyle = new Notification.BigTextStyle();
        bigTextStyle.setBuilder(mBuilder);
        bigTextStyle.bigText(bigText);

        assert mMetadata != null;
        return new ChromeNotification(bigTextStyle.build(), mMetadata);
    }

    @Override
    public Notification build() {
        return mBuilder.build();
    }

    @Override
    public ChromeNotification buildChromeNotification() {
        assert mMetadata != null;
        return new ChromeNotification(build(), mMetadata);
    }
}
