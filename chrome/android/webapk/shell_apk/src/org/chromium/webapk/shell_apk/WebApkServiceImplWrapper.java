// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.RequiresApi;

import org.chromium.webapk.lib.runtime_library.IWebApkApi;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * A wrapper class of {@link org.chromium.webapk.lib.runtime_library.WebApkServiceImpl} that
 * provides additional functionality when the runtime library hasn't been updated.
 */
public class WebApkServiceImplWrapper extends IWebApkApi.Stub {
    private static final String TAG = "cr_WebApkServiceImplWrapper";

    /** The channel id of the WebAPK. */
    private static final String DEFAULT_NOTIFICATION_CHANNEL_ID = "default_channel_id";

    private static final String FUNCTION_NAME_NOTIFY_NOTIFICATION =
            "TRANSACTION_notifyNotification";

    private static final String FUNCTION_NAME_CHECK_NOTIFICATION_PERMISSION =
            "TRANSACTION_checkNotificationPermission";

    private static final String FUNCTION_NAME_REQUEST_NOTIFICATION_PERMISSION =
            "TRANSACTION_requestNotificationPermission";

    /**
     * Uid of only application allowed to call the service's methods. If an application with a
     * different uid calls the service, the service throws a RemoteException.
     */
    private final int mHostUid;

    /**
     * The {@link org.chromium.webapk.lib.runtime_library.WebApkServiceImpl} that this class wraps.
     */
    private IBinder mIBinderDelegate;

    private Context mContext;

    public WebApkServiceImplWrapper(Context context, IBinder delegate, int hostBrowserUid) {
        mContext = context;
        mIBinderDelegate = delegate;
        mHostUid = hostBrowserUid;
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

        // For methods that we want to handle we defer to our parent's onTransact which will
        // dispatch to the method implementations in this class.
        if (code == getApiCode(FUNCTION_NAME_NOTIFY_NOTIFICATION)
                || code == getApiCode(FUNCTION_NAME_CHECK_NOTIFICATION_PERMISSION)
                || code == getApiCode(FUNCTION_NAME_REQUEST_NOTIFICATION_PERMISSION)) {
            return super.onTransact(code, data, reply, flags);
        }

        return delegateOnTransactMethod(code, data, reply, flags);
    }

    @Override
    public int getSmallIconId() {
        Log.w(TAG, "Should NOT reach WebApkServiceImplWrapper#getSmallIconId().");
        return -1;
    }

    @Override
    public boolean notificationPermissionEnabled() throws RemoteException {
        Log.w(TAG, "Should NOT reach WebApkServiceImplWrapper#notificationPermissionEnabled().");
        return false;
    }

    @Override
    @SuppressWarnings("NewApi")
    public void notifyNotification(String platformTag, int platformID, Notification notification) {
        // The WebApkServiceImplWrapper was introduced at the same time when WebAPKs target SDK 26.
        // That means, we don't need to check whether the target SDK is less than 26 in a WebAPK
        // that has a WebApkServiceImplWrapper class.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ensureNotificationChannelExists();
            notification = rebuildNotificationWithChannelId(mContext, notification);
        }
        delegateNotifyNotification(platformTag, platformID, notification);
    }

    @Override
    public void cancelNotification(String platformTag, int platformID) {
        Log.w(TAG, "Should NOT reach WebApkServiceImplWrapper#cancelNotification(String, int).");
    }

    @Override
    public void notifyNotificationWithChannel(
            String platformTag, int platformID, Notification notification, String channelName) {
        Log.w(
                TAG,
                "Should NOT reach WebApkServiceImplWrapper#notifyNotificationWithChannel("
                        + "String, int, Notification, String)");
    }

    @Override
    public boolean finishAndRemoveTaskSdk23() {
        Log.w(TAG, "Should NOT reach WebApkServiceImplWrapper#finishAndRemoveTaskSdk23()");
        return false;
    }

    @Override
    public @PermissionStatus int checkNotificationPermission() {
        boolean enabled = getNotificationManager().areNotificationsEnabled();

        @PermissionStatus int status = enabled ? PermissionStatus.ALLOW : PermissionStatus.BLOCK;
        if (status == PermissionStatus.BLOCK
                && !PrefUtils.hasRequestedNotificationPermission(mContext)
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            status = PermissionStatus.ASK;
        }
        return status;
    }

    @Override
    public PendingIntent requestNotificationPermission(String channelName, String channelId) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            Log.w(TAG, "Cannot request notification permission before Android T.");
            return null;
        }

        return NotificationPermissionRequestActivity.createPermissionRequestPendingIntent(
                mContext, channelName, channelId);
    }

    /** Creates a WebAPK notification channel on Android O+ if one does not exist. */
    protected void ensureNotificationChannelExists() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel =
                    new NotificationChannel(
                            DEFAULT_NOTIFICATION_CHANNEL_ID,
                            WebApkUtils.getNotificationChannelName(mContext),
                            NotificationManager.IMPORTANCE_DEFAULT);
            getNotificationManager().createNotificationChannel(channel);
        }
    }

    protected int getApiCode(String name) {
        if (mIBinderDelegate == null) return -1;

        try {
            Field f = mIBinderDelegate.getClass().getSuperclass().getDeclaredField(name);
            f.setAccessible(true);
            return (int) f.get(null);
        } catch (Exception e) {
            e.printStackTrace();
        }

        return -1;
    }

    /** Calls the delegate's {@link onTransact()} method via reflection. */
    private boolean delegateOnTransactMethod(int code, Parcel data, Parcel reply, int flags)
            throws RemoteException {
        if (mIBinderDelegate == null) return false;

        try {
            Method onTransactMethod =
                    mIBinderDelegate
                            .getClass()
                            .getMethod(
                                    "onTransact",
                                    new Class[] {int.class, Parcel.class, Parcel.class, int.class});
            onTransactMethod.setAccessible(true);
            return (boolean) onTransactMethod.invoke(mIBinderDelegate, code, data, reply, flags);
        } catch (Exception e) {
            e.printStackTrace();
        }

        return false;
    }

    /** Rebuilds a notification with channel ID from the given notification object. */
    @RequiresApi(Build.VERSION_CODES.O)
    private static Notification rebuildNotificationWithChannelId(
            Context context, Notification notification) {
        Notification.Builder builder = Notification.Builder.recoverBuilder(context, notification);
        builder.setChannelId(DEFAULT_NOTIFICATION_CHANNEL_ID);
        return builder.build();
    }

    /** Calls the delegate's {@link notifyNotification} method via reflection. */
    private void delegateNotifyNotification(
            String platformTag, int platformID, Notification notification) {
        if (mIBinderDelegate == null) return;

        try {
            Method notifyMethod =
                    mIBinderDelegate
                            .getClass()
                            .getMethod(
                                    "notifyNotification",
                                    new Class[] {String.class, int.class, Notification.class});
            notifyMethod.setAccessible(true);
            notifyMethod.invoke(mIBinderDelegate, platformTag, platformID, notification);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private NotificationManager getNotificationManager() {
        return (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
    }
}
