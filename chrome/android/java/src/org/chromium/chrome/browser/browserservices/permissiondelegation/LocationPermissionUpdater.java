// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.ComponentName;

import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

import javax.inject.Inject;
import javax.inject.Singleton;

/**
 * This class updates the location permission for an Origin based on the location permission
 * that the linked TWA has in Android.
 *
 * TODO(eirage): Add a README.md for Location Delegation.
 */
@Singleton
public class LocationPermissionUpdater {
    private static final String TAG = "TWALocations";

    private static final @ContentSettingsType.EnumType int TYPE = ContentSettingsType.GEOLOCATION;

    private final InstalledWebappPermissionManager mPermissionManager;
    private final TrustedWebActivityClient mTrustedWebActivityClient;
    private final TrustedWebActivityUmaRecorder mUmaRecorder;

    @Inject
    public LocationPermissionUpdater(
            InstalledWebappPermissionManager permissionManager,
            TrustedWebActivityClient trustedWebActivityClient,
            TrustedWebActivityUmaRecorder umaRecorder) {
        mPermissionManager = permissionManager;
        mTrustedWebActivityClient = trustedWebActivityClient;
        mUmaRecorder = umaRecorder;
    }

    /**
     * If the uninstalled client app results in there being no more TrustedWebActivityService
     * for the origin, or the client app does not support location delegation, return the
     * origin's location permission to what it was before any client app was installed.
     */
    void onClientAppUninstalled(Origin origin) {
        mPermissionManager.resetStoredPermission(origin, TYPE);
    }

    /**
     * To be called when a client app is requesting location. It check and sets the location
     * permission for that origin to client app's Android location permission if a
     * TrustedWebActivityService is found, and the TWAService supports location permission.
     */
    void checkPermission(Origin origin, String lastCommitedUrl, long callback) {
        mTrustedWebActivityClient.checkLocationPermission(
                lastCommitedUrl,
                new TrustedWebActivityClient.PermissionCallback() {
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

    private void updatePermission(
            Origin origin,
            long callback,
            ComponentName app,
            @ContentSettingValues int settingValue) {
        boolean enabled = settingValue == ContentSettingValues.ALLOW;
        mPermissionManager.updatePermission(origin, app.getPackageName(), TYPE, settingValue);
        Log.d(TAG, "Updating origin location permissions to: %b", enabled);

        InstalledWebappBridge.runPermissionCallback(callback, settingValue);
    }
}
