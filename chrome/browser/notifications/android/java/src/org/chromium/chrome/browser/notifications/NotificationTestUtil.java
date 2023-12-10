// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Icon;
import android.os.Bundle;

/** Utils for Android notification tests. */
public class NotificationTestUtil {
    @SuppressWarnings("deprecation") // for Notification.icon
    public static Bitmap getSmallIconFromNotification(Context context, Notification notification) {
        return getBitmapFromIcon(context, notification.getSmallIcon());
    }

    @SuppressWarnings("deprecation") // for Notification.largeIcon
    public static Bitmap getLargeIconFromNotification(Context context, Notification notification) {
        return getBitmapFromIcon(context, notification.getLargeIcon());
    }

    public static Bitmap getBitmapFromIcon(Context context, Icon icon) {
        return ((BitmapDrawable) icon.loadDrawable(context)).getBitmap();
    }

    static Notification.Action[] getActions(Notification notification) {
        return notification.actions;
    }

    public static CharSequence getActionTitle(Notification.Action action) {
        return action.title;
    }

    static Bundle getExtras(Notification notification) {
        return notification.extras;
    }

    static String getExtraTitle(Notification notification) {
        return getExtras(notification).getString(Notification.EXTRA_TITLE);
    }

    static String getExtraText(Notification notification) {
        return getExtras(notification).getString(Notification.EXTRA_TEXT);
    }

    static String getExtraSubText(Notification notification) {
        return getExtras(notification).getString(Notification.EXTRA_SUB_TEXT);
    }

    static Bitmap getExtraPicture(Notification notification) {
        return (Bitmap) getExtras(notification).get(Notification.EXTRA_PICTURE);
    }

    static boolean getExtraShownWhen(Notification notification) {
        return getExtras(notification).getBoolean(Notification.EXTRA_SHOW_WHEN);
    }
}
