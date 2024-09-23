// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.content.Context;

import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

/** Builds a notification using the standard Notification.BigTextStyle layout. */
public class StandardNotificationBuilder extends NotificationBuilderBase {
    private final Context mContext;

    public StandardNotificationBuilder(Context context) {
        super(context.getResources());
        mContext = context;
    }

    @Override
    public NotificationWrapper build(NotificationMetadata metadata) {
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        mChannelId, metadata);

        builder.setContentTitle(mTitle);
        builder.setContentText(mBody);
        builder.setSubText(mOrigin);
        builder.setTicker(mTickerText);
        if (mImage != null) {
            builder.setBigPictureStyle(mImage, mBody);
        } else {
            builder.setBigTextStyle(mBody);
        }
        builder.setLargeIcon(getNormalizedLargeIcon());
        setStatusBarIcon(builder, mSmallIconId, mSmallIconBitmapForStatusBar);
        if (mExtras != null) {
            builder.addExtras(mExtras);
        }
        builder.setContentIntent(mContentIntent);
        if (mDeleteIntentActionType != NotificationUmaTracker.ActionType.UNKNOWN) {
            builder.setDeleteIntent(mDeleteIntent, mDeleteIntentActionType);
        } else {
            builder.setDeleteIntent(mDeleteIntent);
        }
        for (Action action : mActions) {
            addActionToBuilder(builder, action);
        }
        for (Action settingsAction : mSettingsActions) {
            addActionToBuilder(builder, settingsAction);
        }
        builder.setPriorityBeforeO(mPriority);
        builder.setDefaults(mDefaults);
        if (mVibratePattern != null) builder.setVibrate(mVibratePattern);
        builder.setSilent(mSilent);
        if (mTimestamp >= 0) {
            builder.setWhen(mTimestamp);
            builder.setShowWhen(true);
        } else {
            builder.setShowWhen(false);
        }
        if (mTimeoutAfterMs > 0) {
            builder.setTimeoutAfter(mTimeoutAfterMs);
        }
        builder.setOnlyAlertOnce(!mRenotify);
        setGroupOnBuilder(builder, mOrigin);
        builder.setPublicVersion(createPublicNotification(mContext));
        return builder.buildNotificationWrapper();
    }
}
