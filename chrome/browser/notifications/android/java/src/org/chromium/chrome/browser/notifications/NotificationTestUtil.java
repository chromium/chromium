// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Notification;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Bundle;

/**
 * Utils for Android notification tests.
 */
public class NotificationTestUtil {
    @SuppressWarnings("deprecation") // for Notification.icon
    public static Bitmap getSmallIconFromNotification(Context context, Notification notification) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return getBitmapFromIcon(context, notification.getSmallIcon());
        } else {
            return BitmapFactory.decodeResource(context.getResources(), notification.icon);
        }
    }

    @SuppressWarnings("deprecation") // for Notification.largeIcon
    public static Bitmap getLargeIconFromNotification(Context context, Notification notification) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return getBitmapFromIcon(context, notification.getLargeIcon());
        } else {
            return notification.largeIcon;
        }
    }

    @TargetApi(Build.VERSION_CODES.M) // for Icon.loadDrawable()
    public static Bitmap getBitmapFromIcon(Context context, Icon icon) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
        return ((BitmapDrawable) icon.loadDrawable(context)).getBitmap();
    }

    @SuppressLint("NewApi") // Notification.actions is hidden in Jellybean
    static Notification.Action[] getActions(Notification notification) {
        return notification.actions;
    }

    @SuppressLint("NewApi") // Notification.Action is hidden in Jellybean
    public static CharSequence getActionTitle(Notification.Action action) {
        return action.title;
    }

    @SuppressLint("NewApi") // Notification.extras is hidden in Jellybean
    static Bundle getExtras(Notification notification) {
        return notification.extras;
    }

    @SuppressLint("InlinedApi") // Notification.EXTRA_TITLE is hidden in Jellybean
    static String getExtraTitle(Notification notification) {
        return getExtras(notification).getString(Notification.EXTRA_TITLE);
    }

    @SuppressLint("InlinedApi") // Notification.EXTRA_TEXT is hidden in Jellybean
    static String getExtraText(Notification notification) {
        return getExtras(notification).getString(Notification.EXTRA_TEXT);
    }

    @SuppressLint("InlinedApi") // Notification.EXTRA_SUB_TEXT is hidden in Jellybean
    static String getExtraSubText(Notification notification) {
        return getExtras(notification).getString(Notification.EXTRA_SUB_TEXT);
    }

    @SuppressLint("InlinedApi") // Notification.EXTRA_PICTURE is hidden in Jellybean
    static Bitmap getExtraPicture(Notification notification) {
        return (Bitmap) getExtras(notification).get(Notification.EXTRA_PICTURE);
    }

    @SuppressLint("InlinedApi") // Notification.EXTRA_SHOW_WHEN is hidden in Jellybean
    static boolean getExtraShownWhen(Notification notification) {
        return getExtras(notification).getBoolean(Notification.EXTRA_SHOW_WHEN);
    }
}
