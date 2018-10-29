// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.notifications.NotificationBuilderBase;
import org.chromium.webapk.lib.client.WebApkServiceConnectionManager;
import org.chromium.webapk.lib.runtime_library.IWebApkApi;

/**
 * Provides APIs for browsers to communicate with WebAPK services. Each WebAPK has its own "WebAPK
 * service".
 */
public class WebApkServiceClient {
    // Callback which catches RemoteExceptions thrown due to IWebApkApi failure.
    private abstract static class ApiUseCallback
            implements WebApkServiceConnectionManager.ConnectionCallback {
        public abstract void useApi(IWebApkApi api) throws RemoteException;

        @Override
        public void onConnected(IBinder api) {
            if (api == null) {
                WebApkUma.recordBindToWebApkServiceSucceeded(false);
                return;
            }

            try {
                useApi(IWebApkApi.Stub.asInterface(api));
                WebApkUma.recordBindToWebApkServiceSucceeded(true);
            } catch (RemoteException e) {
                Log.w(TAG, "WebApkAPI use failed.", e);
            }
        }
    }

    /**
     * Keeps the value consistent with {@link
     * org.chromium.webapk.shell_apk.WebApkServiceImplWrapper#DEFAULT_NOTIFICATION_CHANNEL_ID}.
     */
    public static final String CHANNEL_ID_WEBAPKS = "default_channel_id";

    private static final String CATEGORY_WEBAPK_API = "android.intent.category.WEBAPK_API";
    private static final String TAG = "cr_WebApk";

    private static WebApkServiceClient sInstance;

    /** Manages connections between the browser application and WebAPK services. */
    private WebApkServiceConnectionManager mConnectionManager;

    public static WebApkServiceClient getInstance() {
        if (sInstance == null) {
            sInstance = new WebApkServiceClient();
        }
        return sInstance;
    }

    private WebApkServiceClient() {
        mConnectionManager =
                new WebApkServiceConnectionManager(CATEGORY_WEBAPK_API, null /* action */);
    }

    /**
     * Connects to a WebAPK's bound service, builds a notification and hands it over to the WebAPK
     * to display. Handing over the notification makes the notification look like it originated from
     * the WebAPK - not Chrome - in the Android UI.
     */
    public void notifyNotification(final String webApkPackage,
            final NotificationBuilderBase notificationBuilder, final String platformTag,
            final int platformID) {
        final ApiUseCallback connectionCallback = new ApiUseCallback() {
            @Override
            public void useApi(IWebApkApi api) throws RemoteException {
                fallbackToWebApkIconIfNecessary(notificationBuilder, webApkPackage,
                        api.getSmallIconId());

                boolean notificationPermissionEnabled = api.notificationPermissionEnabled();
                if (notificationPermissionEnabled) {
                    String channelName = null;
                    if (webApkTargetsAtLeastO(webApkPackage)) {
                        notificationBuilder.setChannelId(CHANNEL_ID_WEBAPKS);
                        channelName = ContextUtils.getApplicationContext().getString(
                                org.chromium.chrome.R.string.webapk_notification_channel_name);
                    }
                    api.notifyNotificationWithChannel(
                            platformTag, platformID, notificationBuilder.build(), channelName);
                }
                WebApkUma.recordNotificationPermissionStatus(notificationPermissionEnabled);
            }
        };

        mConnectionManager.connect(
                ContextUtils.getApplicationContext(), webApkPackage, connectionCallback);
    }

    private void fallbackToWebApkIconIfNecessary(NotificationBuilderBase builder,
            String webApkPackage, int iconId) {
        if (!builder.hasSmallIconForContent()) {
            builder.setContentSmallIconForTrustedRemoteApp(iconId, webApkPackage);
        }
        if (!builder.hasStatusBarIconBitmap()) {
            builder.setStatusBarIconForTrustedRemoteApp(iconId, webApkPackage);
        }
    }

    /** Cancels notification previously shown by WebAPK. */
    public void cancelNotification(
            String webApkPackage, final String platformTag, final int platformID) {
        final ApiUseCallback connectionCallback = new ApiUseCallback() {
            @Override
            public void useApi(IWebApkApi api) throws RemoteException {
                api.cancelNotification(platformTag, platformID);
            }
        };

        mConnectionManager.connect(
                ContextUtils.getApplicationContext(), webApkPackage, connectionCallback);
    }

    /** Disconnects all the connections to WebAPK services. */
    public static void disconnectAll() {
        if (sInstance == null) return;

        sInstance.mConnectionManager.disconnectAll(ContextUtils.getApplicationContext());
    }

    /** Returns whether the WebAPK targets SDK 26+. */
    private boolean webApkTargetsAtLeastO(String webApkPackage) {
        try {
            ApplicationInfo info =
                    ContextUtils.getApplicationContext().getPackageManager().getApplicationInfo(
                            webApkPackage, 0);
            return info.targetSdkVersion >= Build.VERSION_CODES.O;
        } catch (PackageManager.NameNotFoundException e) {
        }

        return false;
    }
}
