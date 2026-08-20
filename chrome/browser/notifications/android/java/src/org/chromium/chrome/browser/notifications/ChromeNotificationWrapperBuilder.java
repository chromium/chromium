// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.media.session.MediaSessionCompat;
import android.widget.RemoteViews;

import androidx.core.app.NotificationCompat;
import androidx.core.graphics.drawable.IconCompat;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/**
 * Wraps {@link NotificationCompat.Builder} and adds UMA telemetry by way of {@link
 * NotificationIntentInterceptor}.
 */
@NullMarked
public class ChromeNotificationWrapperBuilder implements NotificationWrapperBuilder {
    private static final String TAG = "ChromeNotifBuilder";

    private final Context mContext;
    private final NotificationCompat.Builder mBuilder;
    private final @Nullable NotificationMetadata mMetadata;
    private boolean mIsSilent;

    ChromeNotificationWrapperBuilder(
            Context context,
            @Nullable String channelId,
            ChannelsInitializer channelsInitializer,
            @Nullable NotificationMetadata metadata) {
        mContext = context;
        if (channelId != null) {
            channelsInitializer.ensureInitialized(channelId);
            mBuilder = new NotificationCompat.Builder(mContext, channelId);
        } else {
            mBuilder = new NotificationCompat.Builder(mContext);
        }
        mMetadata = metadata;
        if (metadata != null) {
            mBuilder.setDeleteIntent(
                    NotificationIntentInterceptor.getDefaultDeletePendingIntent(metadata));
        }
    }

    @Override
    public NotificationWrapperBuilder setAutoCancel(boolean autoCancel) {
        mBuilder.setAutoCancel(autoCancel);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setContentIntent(@Nullable PendingIntent contentIntent) {
        mBuilder.setContentIntent(contentIntent);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setContentIntent(
            @Nullable PendingIntentProvider contentIntent) {
        if (mMetadata == null || contentIntent == null) {
            return setContentIntent(
                    contentIntent != null ? contentIntent.getPendingIntent() : null);
        }
        PendingIntent pendingIntent =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        /* actionType= */ NotificationUmaTracker.ActionType.UNKNOWN,
                        mMetadata,
                        contentIntent);
        return setContentIntent(pendingIntent);
    }

    @Override
    public NotificationWrapperBuilder setContentTitle(@Nullable CharSequence title) {
        mBuilder.setContentTitle(title);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setContentText(@Nullable CharSequence text) {
        mBuilder.setContentText(text);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSmallIcon(int icon) {
        mBuilder.setSmallIcon(icon);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSmallIcon(Icon icon) {
        mBuilder.setSmallIcon(IconCompat.createFromIcon(mContext, icon));
        return this;
    }

    @Override
    public NotificationWrapperBuilder setColor(int argb) {
        mBuilder.setColor(argb);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setTicker(@Nullable CharSequence text) {
        mBuilder.setTicker(text);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setLocalOnly(boolean localOnly) {
        mBuilder.setLocalOnly(localOnly);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setGroup(String group) {
        mBuilder.setGroup(group);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setGroupSummary(boolean isGroupSummary) {
        mBuilder.setGroupSummary(isGroupSummary);
        return this;
    }

    @Override
    public NotificationWrapperBuilder addExtras(Bundle extras) {
        mBuilder.addExtras(extras);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setOngoing(boolean ongoing) {
        mBuilder.setOngoing(ongoing);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setVisibility(int visibility) {
        mBuilder.setVisibility(visibility);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setShowWhen(boolean showWhen) {
        mBuilder.setShowWhen(showWhen);
        return this;
    }

    @Override
    public NotificationWrapperBuilder addAction(
            int icon, CharSequence title, PendingIntent intent) {
        if (icon != 0) {
            mBuilder.addAction(
                    new NotificationCompat.Action.Builder(
                                    IconCompat.createWithResource(mContext, icon), title, intent)
                            .build());
        } else {
            mBuilder.addAction(icon, title, intent);
        }
        return this;
    }

    @Override
    public NotificationWrapperBuilder addAction(
            int icon,
            CharSequence title,
            PendingIntentProvider pendingIntentProvider,
            @NotificationUmaTracker.ActionType int actionType) {
        if (mMetadata == null) {
            return addAction(icon, title, pendingIntentProvider.getPendingIntent());
        }
        PendingIntent pendingIntent =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                        actionType,
                        mMetadata,
                        pendingIntentProvider);
        return addAction(icon, title, pendingIntent);
    }

    @Override
    public NotificationWrapperBuilder addAction(NotificationCompat.Action action) {
        mBuilder.addAction(action);
        return this;
    }

    @Override
    public NotificationWrapperBuilder addAction(
            NotificationCompat.Action action,
            int flags,
            @NotificationUmaTracker.ActionType int actionType,
            int requestCode) {
        if (mMetadata == null) {
            return addAction(action);
        }
        PendingIntent pendingIntent =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                        actionType,
                        mMetadata,
                        new PendingIntentProvider(action.actionIntent, flags, requestCode));
        action.actionIntent = pendingIntent;
        return addAction(action);
    }

    @Override
    public NotificationWrapperBuilder setDeleteIntent(@Nullable PendingIntent intent) {
        mBuilder.setDeleteIntent(intent);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setDeleteIntent(@Nullable PendingIntentProvider intent) {
        if (mMetadata == null || intent == null) {
            return setDeleteIntent(intent != null ? intent.getPendingIntent() : null);
        }
        return setDeleteIntent(
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.DELETE_INTENT,
                        /* actionType= */ NotificationUmaTracker.ActionType.UNKNOWN,
                        mMetadata,
                        intent));
    }

    @Override
    public NotificationWrapperBuilder setDeleteIntent(
            @Nullable PendingIntentProvider intent,
            @NotificationUmaTracker.ActionType int actionType) {
        if (mMetadata == null || intent == null) {
            return setDeleteIntent(intent != null ? intent.getPendingIntent() : null);
        }
        // As `actionType` will be part of the `requestCode` that `NotificationIntentInterceptor`
        // generates, the below wrapper `PendingIntent` will not be `Intent.filterEquals` to the
        // one above.
        return setDeleteIntent(
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                        actionType,
                        mMetadata,
                        intent));
    }

    @Override
    public NotificationWrapperBuilder setPriorityBeforeO(int pri) {
        mBuilder.setPriority(pri);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setProgress(int max, int percentage, boolean indeterminate) {
        mBuilder.setProgress(max, percentage, indeterminate);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSubText(@Nullable CharSequence text) {
        mBuilder.setSubText(text);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setWhen(long time) {
        mBuilder.setWhen(time);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setLargeIcon(@Nullable Bitmap icon) {
        mBuilder.setLargeIcon(icon);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setVibrate(long[] vibratePattern) {
        mBuilder.setVibrate(vibratePattern);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSound(@Nullable Uri sound) {
        mBuilder.setSound(sound);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setSilent(boolean silent) {
        mBuilder.setSilent(silent);
        mIsSilent = silent;
        return this;
    }

    @Override
    public NotificationWrapperBuilder setDefaults(int defaults) {
        mBuilder.setDefaults(defaults);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setOnlyAlertOnce(boolean onlyAlertOnce) {
        mBuilder.setOnlyAlertOnce(onlyAlertOnce);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setPublicVersion(@Nullable Notification publicNotification) {
        mBuilder.setPublicVersion(publicNotification);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setContent(RemoteViews views) {
        mBuilder.setCustomContentView(views);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setBigPictureStyle(
            Bitmap bigPicture, @Nullable CharSequence summaryText) {
        if (bigPicture.getAllocationByteCount() / 1000 > BIG_PICTURE_BITMAP_MAX_SIZE_IN_KB) {
            bigPicture = resizeBitmap(bigPicture, BIG_PICTURE_BITMAP_MAX_SIZE_IN_KB);
        }

        NotificationCompat.BigPictureStyle style =
                new NotificationCompat.BigPictureStyle().bigPicture(bigPicture);
        // Android N doesn't show content text when expanded, so duplicate body text as a  summary
        // for the big picture.
        style.setSummaryText(summaryText);
        mBuilder.setStyle(style);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setBigTextStyle(@Nullable CharSequence bigText) {
        mBuilder.setStyle(new NotificationCompat.BigTextStyle().bigText(bigText));
        return this;
    }

    @Override
    public NotificationWrapperBuilder setMediaStyle(MediaSessionCompat session, int[] actions) {
        androidx.media.app.NotificationCompat.MediaStyle style =
                new androidx.media.app.NotificationCompat.MediaStyle();
        style.setMediaSession(session.getSessionToken());
        style.setShowActionsInCompactView(actions);
        mBuilder.setStyle(style);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setCategory(String category) {
        mBuilder.setCategory(category);
        return this;
    }

    @Override
    public NotificationWrapperBuilder setTimeoutAfter(long ms) {
        mBuilder.setTimeoutAfter(ms);
        return this;
    }

    @Override
    public NotificationWrapper buildWithBigContentView(RemoteViews view) {
        assert mMetadata != null;
        return new NotificationWrapper(
                mBuilder.setCustomBigContentView(view).build(), mMetadata, mIsSilent);
    }

    @Override
    public NotificationWrapper buildWithBigTextStyle(String bigText) {
        NotificationCompat.BigTextStyle bigTextStyle =
                new NotificationCompat.BigTextStyle(mBuilder);
        bigTextStyle.bigText(bigText);

        assert mMetadata != null;
        return new NotificationWrapper(bigTextStyle.build(), mMetadata, mIsSilent);
    }

    @Override
    public @Nullable Notification build() {
        boolean success = false;
        Notification notification = null;
        try {
            notification = mBuilder.build();
            success = true;
        } catch (NullPointerException e) {
            // Android M and L may throw exception, see https://crbug.com/949794.
            Log.e(TAG, "Failed to build notification.", e);
        } finally {
            RecordHistogram.recordBooleanHistogram("Notifications.Android.Build", success);
        }
        return notification;
    }

    @Override
    public NotificationWrapper buildNotificationWrapper() {
        assert mMetadata != null;
        return new NotificationWrapper(build(), mMetadata, mIsSilent);
    }

    /**
     * Scales down {@code bitmap} if its allocation byte count is larger than {@code
     * maxSizeInKiloBytes}.
     */
    private static Bitmap resizeBitmap(Bitmap bitmap, int maxSizeInKiloBytes) {
        int allocationByteCount = bitmap.getAllocationByteCount();
        if (allocationByteCount / 1000 <= maxSizeInKiloBytes) return bitmap;

        float scale = (float) Math.sqrt((double) (maxSizeInKiloBytes * 1000) / allocationByteCount);
        int newWidth = Math.round(bitmap.getWidth() * scale);
        int newHeight = Math.round(bitmap.getHeight() * scale);

        return Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, /* filter= */ true);
    }
}
