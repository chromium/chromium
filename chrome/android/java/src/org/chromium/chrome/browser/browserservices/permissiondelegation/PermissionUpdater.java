// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.Intent;
import android.net.Uri;

import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.components.embedder_support.util.Origin;

/**
 * This class updates the permission for an Origin based on the permission that the linked TWA has
 * in Android. It also reverts the permission back to that the Origin had before a TWA was installed
 * in the case of TWA uninstallation.
 */
public class PermissionUpdater {
    private static final String TAG = "PermissionUpdater";

    private PermissionUpdater() {}

    /**
     * To be called when an origin is verified with a package. It add the delegate app and update
     * the Notification and Location delegation state for that origin if the package handles
     * browsable intents for the origin; otherwise, it does nothing.
     */
    public static void onOriginVerified(Origin origin, String url, String packageName) {
        // If the client doesn't handle browsable Intents for the URL, we don't do anything special
        // for the origin.
        if (!appHandlesBrowsableIntent(packageName, Uri.parse(url))) {
            Log.d(TAG, "Package does not handle Browsable Intents for the origin.");
            return;
        }

        InstalledWebappPermissionManager.addDelegateApp(origin, packageName);

        NotificationPermissionUpdater.onOriginVerified(origin, url, packageName);
    }

    public static void onWebApkLaunch(Origin origin, String packageName) {
        NotificationPermissionUpdater.onWebApkLaunch(origin, packageName);
    }

    public static void onClientAppUninstalled(Origin origin) {
        NotificationPermissionUpdater.onClientAppUninstalled(origin);
        LocationPermissionUpdater.onClientAppUninstalled(origin);
    }

    private static boolean appHandlesBrowsableIntent(String packageName, Uri uri) {
        Intent browsableIntent = new Intent();
        browsableIntent.setPackage(packageName);
        browsableIntent.setData(uri);
        browsableIntent.setAction(Intent.ACTION_VIEW);
        browsableIntent.addCategory(Intent.CATEGORY_BROWSABLE);

        return PackageManagerUtils.resolveActivity(browsableIntent, 0) != null;
    }

    static void getLocationPermission(Origin origin, String lastCommittedUrl, long callback) {
        LocationPermissionUpdater.checkPermission(origin, lastCommittedUrl, callback);
    }

    static void requestNotificationPermission(
            Origin origin, String lastCommittedUrl, long callback) {
        NotificationPermissionUpdater.requestPermission(origin, lastCommittedUrl, callback);
    }
}
