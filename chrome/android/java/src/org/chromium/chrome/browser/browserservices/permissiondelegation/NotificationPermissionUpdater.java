// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.ComponentName;

import androidx.annotation.WorkerThread;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import javax.inject.Inject;
import javax.inject.Singleton;

/**
 * This class updates the notification permission for an Origin based on the notification permission
 * that the linked TWA has in Android. It also reverts the notification permission back to that the
 * Origin had before a TWA was installed in the case of TWA uninstallation.
 *
 * TODO(peconn): Add a README.md for Notification Delegation.
 */
@Singleton
public class NotificationPermissionUpdater {
    private static final String TAG = "TWANotifications";

    private static final @ContentSettingsType int TYPE = ContentSettingsType.NOTIFICATIONS;

    private final TrustedWebActivityPermissionManager mPermissionManager;
    private final TrustedWebActivityClient mTrustedWebActivityClient;

    @Inject
    public NotificationPermissionUpdater(
            TrustedWebActivityPermissionManager permissionManager,
            TrustedWebActivityClient trustedWebActivityClient) {
        mPermissionManager = permissionManager;
        mTrustedWebActivityClient = trustedWebActivityClient;
    }

    /**
     * To be called when an origin is verified with a package. It sets the notification permission
     * for that origin according to the following:
     * - If a TrustedWebActivityService is found, it updates Chrome's notification permission for
     * that origin to match Android's notification permission for the package.
     * - Otherwise, it does nothing.
     */
    public void onOriginVerified(Origin origin, String packageName) {
        // It's important to note here that the client we connect to to check for the notification
        // permission may not be the client that triggered this method call.

        // The function passed to this method call may not be executed in the case of the app not
        // having a TrustedWebActivityService. That's fine because we only want to update the
        // permission if a TrustedWebActivityService exists.
        if (CachedFeatureFlags.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NOTIFICATION_PERMISSION_DELEGATION)) {
            mTrustedWebActivityClient.checkNotificationPermissionSetting(
                    origin, (app, settingValue) -> {
                        updatePermission(origin, /*callback=*/0, app, settingValue);
                    });
        } else {
            mTrustedWebActivityClient.checkNotificationPermission(
                    origin, (app, enabled) -> updatePermission(origin, app, enabled));
        }
    }

    /**
     * If the uninstalled client app results in there being no more TrustedWebActivityService for
     * the origin, return the origin's notification permission to what it was before any client
     * app was installed.
     */
    public void onClientAppUninstalled(Origin origin) {
        // See if there is any other app installed that could handle the notifications (and update
        // to that apps notification permission if it exists).
        if (CachedFeatureFlags.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NOTIFICATION_PERMISSION_DELEGATION)) {
            mTrustedWebActivityClient.checkNotificationPermissionSetting(
                    origin, new TrustedWebActivityClient.PermissionCallback() {
                        @Override
                        public void onPermission(
                                ComponentName app, @ContentSettingValues int settingValue) {
                            updatePermission(origin, /*callback=*/0, app, settingValue);
                        }

                        @Override
                        public void onNoTwaFound() {
                            mPermissionManager.unregister(origin);
                        }
                    });
        } else {
            mTrustedWebActivityClient.checkNotificationPermission(
                    origin, new TrustedWebActivityClient.PermissionCheckCallback() {
                        @Override
                        public void onPermissionCheck(ComponentName answeringApp, boolean enabled) {
                            updatePermission(origin, answeringApp, enabled);
                        }

                        @Override
                        public void onNoTwaFound() {
                            mPermissionManager.unregister(origin);
                        }
                    });
        }
    }

    /**
     * Called when a client app is requesting notification permission. If a
     * TrustedWebActivityService is found for the given origin, this requests the client app's
     * Android notification permission.
     */
    void requestPermission(Origin origin, long callback) {
        mTrustedWebActivityClient.requestNotificationPermission(
                origin, new TrustedWebActivityClient.PermissionCallback() {
                    private boolean mCalled;
                    @Override
                    public void onPermission(
                            ComponentName app, @ContentSettingValues int settingValue) {
                        if (mCalled) return;
                        mCalled = true;
                        updatePermission(origin, callback, app, settingValue);
                    }

                    @Override
                    public void onNoTwaFound() {
                        if (mCalled) return;
                        mCalled = true;
                        mPermissionManager.resetStoredPermission(origin, TYPE);
                        InstalledWebappBridge.runPermissionCallback(
                                callback, ContentSettingValues.BLOCK);
                    }
                });
    }

    @WorkerThread
    // TODO(crbug.com/1320272): Delete this method once the new flow has shipped.
    private void updatePermission(Origin origin, ComponentName app, boolean enabled) {
        // This method will be called by the TrustedWebActivityClient on a background thread, so
        // hop back over to the UI thread to deal with the result.
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> {
            Log.d(TAG, "Updating notification permission to: %b", enabled);
            mPermissionManager.updatePermission(origin, app.getPackageName(), TYPE, enabled);
        });
    }

    @WorkerThread
    private void updatePermission(Origin origin, long callback, ComponentName app,
            @ContentSettingValues int settingValue) {
        // This method will be called by the TrustedWebActivityClient on a background thread, so
        // hop back over to the UI thread to deal with the result.
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> {
            Log.d(TAG, "Updating notification permission to: %d", settingValue);
            mPermissionManager.updatePermission(origin, app.getPackageName(), TYPE, settingValue);
            InstalledWebappBridge.runPermissionCallback(callback, settingValue);
        });
    }
}
