// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.ComponentName;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.chrome.browser.webapps.WebApkServiceClient;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.webapk.lib.client.WebApkValidator;

/**
 * This class updates the notification permission for an Origin based on the notification permission
 * that the linked TWA has in Android. It also reverts the notification permission back to that the
 * Origin had before a TWA was installed in the case of TWA uninstallation.
 *
 * <p>TODO(peconn): Add a README.md for Notification Delegation.
 */
public class NotificationPermissionUpdater {
    private static final String TAG = "PermissionUpdater";

    private static final @ContentSettingsType.EnumType int TYPE = ContentSettingsType.NOTIFICATIONS;

    private NotificationPermissionUpdater() {}

    /**
     * To be called when an origin is verified with a package. It sets the notification permission
     * for that origin according to the following: - If a TrustedWebActivityService is found, it
     * updates Chrome's notification permission for that origin to match Android's notification
     * permission for the package. - Otherwise, it does nothing.
     */
    public static void onOriginVerified(Origin origin, String url, String packageName) {
        // It's important to note here that the client we connect to to check for the notification
        // permission may not be the client that triggered this method call.

        // The function passed to this method call may not be executed in the case of the app not
        // having a TrustedWebActivityService. That's fine because we only want to update the
        // permission if a TrustedWebActivityService exists.
        TrustedWebActivityClient.getInstance()
                .checkNotificationPermission(
                        url,
                        (app, settingValue) ->
                                updatePermission(
                                        origin,
                                        /* callback= */ 0,
                                        app.getPackageName(),
                                        settingValue));
    }

    public static void onWebApkLaunch(Origin origin, String packageName) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return;
        }
        WebApkServiceClient.getInstance()
                .checkNotificationPermission(
                        packageName,
                        settingValue ->
                                updatePermission(
                                        origin, /* callback= */ 0, packageName, settingValue));
    }

    /**
     * If the uninstalled client app results in there being no more TrustedWebActivityService for
     * the origin, return the origin's notification permission to what it was before any client app
     * was installed.
     */
    public static void onClientAppUninstalled(Origin origin) {
        // See if there is any other app installed that could handle the notifications (and update
        // to that apps notification permission if it exists).
        TrustedWebActivityClient.getInstance()
                .checkNotificationPermission(
                        origin.toString(),
                        new TrustedWebActivityClient.PermissionCallback() {
                            @Override
                            public void onPermission(
                                    ComponentName app, @ContentSettingValues int settingValue) {
                                updatePermission(
                                        origin,
                                        /* callback= */ 0,
                                        app.getPackageName(),
                                        settingValue);
                            }

                            @Override
                            public void onNoTwaFound() {
                                InstalledWebappPermissionManager.unregister(origin);
                            }
                        });
    }

    /**
     * Called when a web page with an installed app is requesting notification permission. This
     * first looks for a TWA and if that fails it looks for a WebAPK. When an app is found this
     * requests the app's Android notification permission. Calling this method only makes sense from
     * Android T, there is no permission dialog for showing notifications in earlier versions.
     */
    static void requestPermission(Origin origin, String lastCommittedUrl, long callback) {
        assert (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                : "Cannot request notification permission before Android T";
        TrustedWebActivityClient.getInstance()
                .requestNotificationPermission(
                        lastCommittedUrl,
                        new TrustedWebActivityClient.PermissionCallback() {
                            private boolean mCalled;

                            @Override
                            public void onPermission(
                                    ComponentName app, @ContentSettingValues int settingValue) {
                                if (mCalled) return;
                                mCalled = true;
                                TrustedWebActivityUmaRecorder
                                        .recordNotificationPermissionRequestResult(settingValue);
                                updatePermission(
                                        origin, callback, app.getPackageName(), settingValue);
                            }

                            @Override
                            public void onNoTwaFound() {
                                if (mCalled) return;
                                mCalled = true;
                                findWebApkPackageName(
                                        lastCommittedUrl,
                                        packageName ->
                                                requestPermissionFromWebApk(
                                                        origin, callback, packageName));
                            }
                        });
    }

    private static void requestPermissionFromWebApk(
            Origin origin, long callback, @Nullable String packageName) {
        if (TextUtils.isEmpty(packageName)) {
            InstalledWebappPermissionManager.resetStoredPermission(origin, TYPE);
            InstalledWebappBridge.runPermissionCallback(callback, ContentSettingValues.BLOCK);
            return;
        }

        WebApkServiceClient.getInstance()
                .requestNotificationPermission(
                        packageName,
                        settingValue -> {
                            WebApkUmaRecorder.recordNotificationPermissionRequestResult(
                                    settingValue);
                            updatePermission(origin, callback, packageName, settingValue);
                        });
    }

    /**
     * Finds a WebAPK that can handle the URL and is backed by Chrome. The package name will be null
     * if no WebAPK could be found matching these criteria. Note that a WebAPK uses a scope URL
     * which may contain a path. An origin has no path and would not fall within such a scope. So,
     * you must pass a more complete URL into this method to get matches for those cases.
     */
    private static void findWebApkPackageName(String url, Callback<String> packageNameCallback) {
        String webApkPackageName =
                WebApkValidator.queryFirstWebApkPackage(ContextUtils.getApplicationContext(), url);
        if (webApkPackageName == null) {
            packageNameCallback.onResult(null);
            return;
        }
        ChromeWebApkHost.checkChromeBacksWebApkAsync(
                webApkPackageName,
                (doesBrowserBackWebApk, browserPackageName) ->
                        packageNameCallback.onResult(
                                doesBrowserBackWebApk ? webApkPackageName : null));
    }

    private static void updatePermission(
            Origin origin,
            long callback,
            String packageName,
            @ContentSettingValues int settingValue) {
        Log.d(TAG, "Updating notification permission to: %d", settingValue);
        InstalledWebappPermissionManager.updatePermission(origin, packageName, TYPE, settingValue);
        InstalledWebappBridge.runPermissionCallback(callback, settingValue);
    }
}
