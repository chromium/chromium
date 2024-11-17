// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.runtime_library;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.Parcel;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.Log;

/** Implements services offered by the WebAPK to Chrome. */
public class WebApkServiceImpl extends IWebApkApi.Stub {

    public static final String KEY_SMALL_ICON_ID = "small_icon_id";
    public static final String KEY_HOST_BROWSER_UID = "host_browser_uid";

    private static final String TAG = "WebApkServiceImpl";

    private final Context mContext;

    /** Id of icon to represent WebAPK notifications in status bar. */
    private final int mSmallIconId;

    /**
     * Uid of only application allowed to call the service's methods. If an application with a
     * different uid calls the service, the service throws a RemoteException.
     */
    private final int mHostUid;

    /**
     * Creates an instance of WebApkServiceImpl.
     *
     * @param bundle Bundle with additional constructor parameters.
     */
    public WebApkServiceImpl(Context context, Bundle bundle) {
        mContext = context;
        mSmallIconId = bundle.getInt(KEY_SMALL_ICON_ID);
        mHostUid = bundle.getInt(KEY_HOST_BROWSER_UID);
        assert mHostUid >= 0;
    }

    @Override
    public boolean onTransact(int code, Parcel data, Parcel reply, int flags)
            throws RemoteException {
        int callingUid = Binder.getCallingUid();
        if (mHostUid != callingUid) {
            throw new RemoteException(
                    "Unauthorized caller "
                            + callingUid
                            + " does not match expected host="
                            + mHostUid);
        }
        return super.onTransact(code, data, reply, flags);
    }

    @Override
    public int getSmallIconId() {
        return mSmallIconId;
    }

    @Override
    public void notifyNotification(String platformTag, int platformID, Notification notification) {
        Log.w(
                TAG,
                "Should NOT reach WebApkServiceImpl#notifyNotification(String, int,"
                        + " Notification).");
    }

    @Override
    public void cancelNotification(String platformTag, int platformID) {
        getNotificationManager().cancel(platformTag, platformID);
    }

    @Override
    public boolean notificationPermissionEnabled() {
        Log.w(
                TAG,
                "Should NOT reach WebApkServiceImpl#notificationPermissionEnabled() because it is"
                        + " deprecated.");
        NotificationManager notificationManager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        return notificationManager.areNotificationsEnabled();
    }

    @SuppressLint("NewApi")
    @Override
    public boolean finishAndRemoveTaskSdk23() {
        ActivityManager manager =
                (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
        String webApkPackageName = mContext.getPackageName();
        for (ActivityManager.AppTask task : manager.getAppTasks()) {
            if (TextUtils.equals(getTaskBaseActivityPackageName(task), webApkPackageName)) {
                task.finishAndRemoveTask();
                return true;
            }
        }
        return false;
    }

    @Override
    public int checkNotificationPermission() {
        Log.w(TAG, "Should NOT reach WebApkServiceImpl#checkNotificationPermission().");
        return -1;
    }

    @Override
    public PendingIntent requestNotificationPermission(String channelName, String channelId) {
        Log.w(
                TAG,
                "Should NOT reach WebApkServiceImpl#requestNotificationPermission(String,"
                        + " String).");
        return null;
    }

    /** Returns the package name of the task's base activity. */
    private static String getTaskBaseActivityPackageName(ActivityManager.AppTask task) {
        try {
            ActivityManager.RecentTaskInfo info = task.getTaskInfo();
            if (info != null && info.baseActivity != null) {
                return info.baseActivity.getPackageName();
            }
        } catch (IllegalArgumentException e) {
        }
        return null;
    }

    @SuppressWarnings("NewApi")
    @Override
    public void notifyNotificationWithChannel(
            String platformTag, int platformID, Notification notification, String channelName) {
        NotificationManager notificationManager = getNotificationManager();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && notification.getChannelId() != null) {
            NotificationChannel channel =
                    new NotificationChannel(
                            notification.getChannelId(),
                            channelName,
                            NotificationManager.IMPORTANCE_DEFAULT);
            notificationManager.createNotificationChannel(channel);
        }

        notificationManager.notify(platformTag, platformID, notification);
    }

    private NotificationManager getNotificationManager() {
        return (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
    }
}
