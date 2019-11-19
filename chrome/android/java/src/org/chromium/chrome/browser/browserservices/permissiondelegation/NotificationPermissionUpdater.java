// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.browserservices.BrowserServicesMetrics;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import javax.inject.Inject;
import javax.inject.Named;
import javax.inject.Singleton;

import androidx.annotation.WorkerThread;

/**
 * This class updates the notification permission for an Origin based on the notification permission
 * that the linked TWA has in Android. It also reverts the notification permission back to that the
 * Origin had before a TWA was installed in the case of TWA uninstallation.
 *
 * TODO(peconn): Add a README.md for Notification Delegation.
 * TODO(peconn): Revert the permission when the TWA is uninstalled.
 * TODO(peconn): Update the permission when a push notification occurs.
 */
@Singleton
public class NotificationPermissionUpdater {
    private static final String TAG = "TWANotifications";

    private final TrustedWebActivityPermissionManager mPermissionManager;
    private final TrustedWebActivityClient mTrustedWebActivityClient;

    @Inject
    public NotificationPermissionUpdater(@Named(APP_CONTEXT) Context context,
            TrustedWebActivityPermissionManager permissionManager,
            TrustedWebActivityClient trustedWebActivityClient) {
        mPermissionManager = permissionManager;
        mTrustedWebActivityClient = trustedWebActivityClient;
    }

    /**
     * To be called when an origin is verified with a package. It sets the notification permission
     * for that origin according to the following:
     * - If the package handles browsable intents for the origin and a TrustedWebActivityService
     *   is found, it updates Chrome's notification permission for that origin to match Android's
     *   notification permission for the package.
     * - Otherwise, it does nothing.
     */
    public void onOriginVerified(Origin origin, String packageName) {
        // If the client doesn't handle browsable Intents for the URL, we don't do anything special
        // for the origin's notifications.
        if (!appHandlesBrowsableIntent(packageName, origin.uri())) {
            Log.d(TAG, "Package does not handle Browsable Intents for the origin");
            return;
        }

        // It's important to note here that the client we connect to to check for the notification
        // permission may not be the client that triggered this method call.

        // The function passed to this method call may not be executed in the case of the app not
        // having a TrustedWebActivityService. That's fine because we only want to update the
        // permission if a TrustedWebActivityService exists.
        mTrustedWebActivityClient.checkNotificationPermission(origin,
                (app, enabled) -> updatePermission(origin, app, enabled));
    }

    /**
     * If the uninstalled client app results in there being no more TrustedWebActivityService for
     * the origin, return the origin's notification permission to what it was before any client
     * app was installed.
     */
    public void onClientAppUninstalled(Origin origin) {
        // See if there is any other app installed that could handle the notifications (and update
        // to that apps notification permission if it exists).
        boolean couldConnect = mTrustedWebActivityClient.checkNotificationPermission(origin,
                (app, enabled) -> updatePermission(origin, app, enabled));

        // If not, we return notification state to what it was before installation.
        if (!couldConnect) {
            mPermissionManager.unregister(origin);
        }
    }

    /**
     * To be called when a notification is delegated to a client and we notice that the client has
     * notifications disabled.
     */
    @WorkerThread
    public static void onDelegatedNotificationDisabled(Origin origin, ComponentName app) {
        // This method is called from TrustedWebActivityClient, which this class requires, so
        // we can't inject this class into TrustedWebActivityClient and we can't set this class as
        // an observer of TrustedWebActivityClient since we aren't guaranteed to be created before
        // TrustedWebActivityClient#notifyNotification is called. So grab an instance of this class
        // out of Dagger.
        // TODO(peconn): Make the lifetimes/dependencies here less ugly.
        ChromeApplication.getComponent().resolveTwaPermissionUpdater()
                .updatePermission(origin, app, false);
    }

    @WorkerThread
    private void updatePermission(Origin origin, ComponentName app, boolean enabled) {
        // This method will be called by the TrustedWebActivityClient on a background thread, so
        // hop back over to the UI thread to deal with the result.
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> {
            mPermissionManager.register(origin, app.getPackageName(), enabled);
            Log.d(TAG, "Updating origin notification permissions to: %b", enabled);
        });
    }

    private boolean appHandlesBrowsableIntent(String packageName, Uri uri) {
        Intent browsableIntent = new Intent();
        browsableIntent.setPackage(packageName);
        browsableIntent.setData(uri);
        browsableIntent.setAction(Intent.ACTION_VIEW);
        browsableIntent.addCategory(Intent.CATEGORY_BROWSABLE);

        try (BrowserServicesMetrics.TimingMetric unused =
                     BrowserServicesMetrics.getBrowsableIntentResolutionTimingContext()) {
            return PackageManagerUtils.resolveActivity(browsableIntent, 0) != null;
        }
    }
}
