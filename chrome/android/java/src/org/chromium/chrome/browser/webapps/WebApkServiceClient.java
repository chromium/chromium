// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Messenger;
import android.os.RemoteException;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.browserservices.permissiondelegation.PermissionStatus;
import org.chromium.chrome.browser.notifications.NotificationBuilderBase;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.webapk.lib.client.WebApkServiceConnectionManager;
import org.chromium.webapk.lib.runtime_library.IWebApkApi;

/**
 * Provides APIs for browsers to communicate with WebAPK services. Each WebAPK has its own "WebAPK
 * service".
 */
public class WebApkServiceClient {
    // Callback which catches RemoteExceptions thrown due to IWebApkApi failure.
    @FunctionalInterface
    private interface ApiUseCallback extends WebApkServiceConnectionManager.ConnectionCallback {
        void useApi(IWebApkApi api) throws RemoteException;

        @Override
        default void onConnected(IBinder api) {
            if (api == null) {
                return;
            }

            try {
                useApi(IWebApkApi.Stub.asInterface(api));
            } catch (RemoteException e) {
                Log.w(TAG, "WebApkAPI use failed.", e);
            }
        }
    }

    @VisibleForTesting
    public static final String CATEGORY_WEBAPK_API = "android.intent.category.WEBAPK_API";

    private static final String TAG = "WebApkServiceClient";

    // An intent extra for a {@link Messenger}.
    private static final String EXTRA_MESSENGER = "messenger";

    // A bundle key for a {@link PermissionStatus}.
    private static final String KEY_PERMISSION_STATUS = "permissionStatus";

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
                new WebApkServiceConnectionManager(
                        TaskTraits.UI_DEFAULT, CATEGORY_WEBAPK_API, /* action= */ null);
    }

    /**
     * Gets the notification permission setting for the package.
     * @param permissionCallback To be called on a background thread with a permission setting, one
     *         of {@link ContentSettingValues}.
     */
    public void checkNotificationPermission(
            String webApkPackage, Callback<Integer> permissionCallback) {
        connect(
                webApkPackage,
                api -> {
                    @ContentSettingValues
                    int settingValue = toContentSettingValue(api.checkNotificationPermission());
                    permissionCallback.onResult(settingValue);
                });
    }

    private static Handler createPermissionHandler(Callback<Integer> permissionCallback) {
        return new Handler(
                Looper.getMainLooper(),
                message -> {
                    @ContentSettingValues
                    int settingValue =
                            toContentSettingValue(
                                    message.getData()
                                            .getInt(KEY_PERMISSION_STATUS, PermissionStatus.BLOCK));
                    permissionCallback.onResult(settingValue);
                    return true;
                });
    }

    /**
     * Requests the notification permission for the package.
     * @param permissionCallback To be called on a background thread with a permission setting, one
     *         of {@link ContentSettingValues}.
     */
    public void requestNotificationPermission(
            String webApkPackage, Callback<Integer> permissionCallback) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            Log.w(TAG, "Requesting notification permission is not supported before T.");
            return;
        }
        connect(
                webApkPackage,
                api -> {
                    String channelName =
                            ContextUtils.getApplicationContext()
                                    .getString(R.string.webapk_notification_channel_name);

                    PendingIntent permissionRequestIntent =
                            api.requestNotificationPermission(
                                    channelName, ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS);
                    if (permissionRequestIntent == null) {
                        permissionCallback.onResult(ContentSettingValues.ASK);
                        return;
                    }

                    Handler handler = createPermissionHandler(permissionCallback);
                    Intent extraIntent = new Intent();
                    extraIntent.putExtra(EXTRA_MESSENGER, new Messenger(handler));
                    try {
                        ActivityOptions options = ActivityOptions.makeBasic();
                        ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartMode(
                                options);
                        permissionRequestIntent.send(
                                ContextUtils.getApplicationContext(),
                                0,
                                extraIntent,
                                null,
                                null,
                                null,
                                options.toBundle());
                    } catch (PendingIntent.CanceledException e) {
                        Log.e(TAG, "The PendingIntent was canceled.", e);
                    }
                });
    }

    /**
     * Connects to a WebAPK's bound service, builds a notification and hands it over to the WebAPK
     * to display. Handing over the notification makes the notification look like it originated from
     * the WebAPK - not Chrome - in the Android UI.
     */
    public void notifyNotification(
            final String originString,
            final String webApkPackage,
            final NotificationBuilderBase notificationBuilder,
            final String platformTag,
            final int platformID) {
        connect(
                webApkPackage,
                api -> {
                    fallbackToWebApkIconIfNecessary(
                            notificationBuilder, webApkPackage, api.getSmallIconId());

                    @ContentSettingValues
                    int settingValue = toContentSettingValue(api.checkNotificationPermission());

                    // See http://crbug.com/1340854. Temporary fallback in case the shell has not
                    // yet been updated to support checkNotificationPermission(). Delete this
                    // after shell v154 has been fully launched.
                    if (settingValue != ContentSettingValues.ALLOW
                            && api.notificationPermissionEnabled()) {
                        Log.d(TAG, "Fallback to notificationPermissionEnabled().");
                        settingValue = ContentSettingValues.ALLOW;
                    }

                    WebApkUmaRecorder.recordNotificationPermissionStatus(settingValue);

                    if (settingValue != ContentSettingValues.ALLOW) {
                        Origin origin = Origin.create(originString);
                        if (origin == null) {
                            Log.w(TAG, "String (%s) could not be parsed as Origin.", originString);
                            return;
                        }
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            InstalledWebappPermissionManager.get()
                                    .updatePermission(
                                            origin,
                                            webApkPackage,
                                            ContentSettingsType.NOTIFICATIONS,
                                            settingValue);
                        }
                        return;
                    }

                    String channelName = null;
                    if (webApkTargetsAtLeastO(webApkPackage)) {
                        notificationBuilder.setChannelId(
                                ChromeChannelDefinitions.CHANNEL_ID_WEBAPKS);
                        channelName =
                                ContextUtils.getApplicationContext()
                                        .getString(R.string.webapk_notification_channel_name);
                    }
                    NotificationMetadata metadata =
                            new NotificationMetadata(
                                    NotificationUmaTracker.SystemNotificationType.WEBAPK,
                                    platformTag,
                                    platformID);

                    api.notifyNotificationWithChannel(
                            platformTag,
                            platformID,
                            notificationBuilder.build(metadata).getNotification(),
                            channelName);
                });
    }

    private void fallbackToWebApkIconIfNecessary(
            NotificationBuilderBase builder, String webApkPackage, int iconId) {
        Bitmap icon = decodeImageResourceFromPackage(webApkPackage, iconId);
        if (!builder.hasSmallIconForContent()) {
            builder.setContentSmallIconForRemoteApp(icon);
        }
        if (!builder.hasStatusBarIconBitmap()) {
            builder.setStatusBarIconForRemoteApp(iconId, icon);
        }
    }

    /** Cancels notification previously shown by WebAPK. */
    public void cancelNotification(
            String webApkPackage, final String platformTag, final int platformID) {
        connect(webApkPackage, api -> api.cancelNotification(platformTag, platformID));
    }

    /** Finishes and removes the WebAPK's task. */
    public void finishAndRemoveTaskSdk23(final Activity activity, WebApkExtras webApkExtras) {
        connect(
                webApkExtras.webApkPackageName,
                api -> {
                    if (activity.isFinishing() || activity.isDestroyed()) return;

                    if (!api.finishAndRemoveTaskSdk23()) {
                        // If |activity| is not the root of the task, hopefully the activities below
                        // this one will close themselves.
                        activity.finish();
                    }
                });
    }

    /** Returns whether there are any WebAPK service API calls in progress. */
    public static boolean hasPendingWork() {
        return sInstance != null && !sInstance.mConnectionManager.didAllConnectCallbacksRun();
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
                    ContextUtils.getApplicationContext()
                            .getPackageManager()
                            .getApplicationInfo(webApkPackage, 0);
            return info.targetSdkVersion >= Build.VERSION_CODES.O;
        } catch (PackageManager.NameNotFoundException e) {
        }

        return false;
    }

    /** Decodes into a Bitmap an Image resource stored in an APK with the given package name. */
    private static @Nullable Bitmap decodeImageResourceFromPackage(
            String packageName, int resourceId) {
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        try {
            Resources resources = packageManager.getResourcesForApplication(packageName);
            return BitmapFactory.decodeResource(resources, resourceId);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
    }

    private void connect(String webApkPackage, ApiUseCallback connectionCallback) {
        mConnectionManager.connect(
                ContextUtils.getApplicationContext(), webApkPackage, connectionCallback);
    }

    private static @ContentSettingValues int toContentSettingValue(
            @PermissionStatus int permissionStatus) {
        if (permissionStatus == PermissionStatus.ALLOW) {
            return ContentSettingValues.ALLOW;
        }

        if (permissionStatus == PermissionStatus.ASK) {
            return ContentSettingValues.ASK;
        }

        return ContentSettingValues.BLOCK;
    }
}
