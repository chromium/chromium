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
        builder.setContentIntent(mContentIntent);
        builder.setDeleteIntent(mDeleteIntent);
        for (Action action : mActions) {
            addActionToBuilder(builder, action);
        }
        if (mSettingsAction != null) {
            addActionToBuilder(builder, mSettingsAction);
        }
        builder.setPriorityBeforeO(mPriority);
        builder.setDefaults(mDefaults);
        if (mVibratePattern != null) builder.setVibrate(mVibratePattern);
        builder.setSilent(mSilent);
        builder.setWhen(mTimestamp);
        builder.setShowWhen(true);
        builder.setOnlyAlertOnce(!mRenotify);
        setGroupOnBuilder(builder, mOrigin);
        builder.setPublicVersion(createPublicNotification(mContext));
        return builder.buildNotificationWrapper();
    }
}
