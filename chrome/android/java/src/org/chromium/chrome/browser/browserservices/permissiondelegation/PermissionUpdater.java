// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.Intent;
import android.net.Uri;

import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.components.embedder_support.util.Origin;

import javax.inject.Inject;
import javax.inject.Singleton;

/**
 * This class updates the permission for an Origin based on the permission that the linked TWA has
 * in Android. It also reverts the permission back to that the Origin had before a TWA was installed
 * in the case of TWA uninstallation.
 */
@Singleton
public class PermissionUpdater {
    private static final String TAG = "PermissionUpdater";

    private final InstalledWebappPermissionManager mPermissionManager;

    private final NotificationPermissionUpdater mNotificationPermissionUpdater;
    private final LocationPermissionUpdater mLocationPermissionUpdater;

    @Inject
    public PermissionUpdater(
            InstalledWebappPermissionManager permissionManager,
            NotificationPermissionUpdater notificationPermissionUpdater,
            LocationPermissionUpdater locationPermissionUpdater) {
        mPermissionManager = permissionManager;
        mNotificationPermissionUpdater = notificationPermissionUpdater;
        mLocationPermissionUpdater = locationPermissionUpdater;
    }

    public static PermissionUpdater get() {
        return ChromeApplicationImpl.getComponent().resolvePermissionUpdater();
    }

    /**
     * To be called when an origin is verified with a package. It add the delegate app and update
     * the Notification and Location delegation state for that origin if the package handles
     * browsable intents for the origin; otherwise, it does nothing.
     */
    public void onOriginVerified(Origin origin, String url, String packageName) {
        // If the client doesn't handle browsable Intents for the URL, we don't do anything special
        // for the origin.
        if (!appHandlesBrowsableIntent(packageName, Uri.parse(url))) {
            Log.d(TAG, "Package does not handle Browsable Intents for the origin.");
            return;
        }

        mPermissionManager.addDelegateApp(origin, packageName);

        mNotificationPermissionUpdater.onOriginVerified(origin, url, packageName);
    }

    public void onWebApkLaunch(Origin origin, String packageName) {
        mNotificationPermissionUpdater.onWebApkLaunch(origin, packageName);
    }

    public void onClientAppUninstalled(Origin origin) {
        mNotificationPermissionUpdater.onClientAppUninstalled(origin);
        mLocationPermissionUpdater.onClientAppUninstalled(origin);
    }

    private boolean appHandlesBrowsableIntent(String packageName, Uri uri) {
        Intent browsableIntent = new Intent();
        browsableIntent.setPackage(packageName);
        browsableIntent.setData(uri);
        browsableIntent.setAction(Intent.ACTION_VIEW);
        browsableIntent.addCategory(Intent.CATEGORY_BROWSABLE);

        return PackageManagerUtils.resolveActivity(browsableIntent, 0) != null;
    }

    void getLocationPermission(Origin origin, String lastCommittedUrl, long callback) {
        mLocationPermissionUpdater.checkPermission(origin, lastCommittedUrl, callback);
    }

    void requestNotificationPermission(Origin origin, String lastCommittedUrl, long callback) {
        mNotificationPermissionUpdater.requestPermission(origin, lastCommittedUrl, callback);
    }
}
