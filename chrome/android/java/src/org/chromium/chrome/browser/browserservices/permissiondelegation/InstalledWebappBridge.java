// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.net.Uri;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Provides Trusted Web Activity Client App permissions for native. The C++ counterpart is the
 * {@code installed_webapp_bridge.h}.
 *
 * Lifecycle: All methods are static.
 * Thread safety: Methods will only be called on the UI thread.
 * Native: Requires native to be loaded.
 */
public class InstalledWebappBridge {
    private static long sNativeInstalledWebappProvider;

    /**
     * A POD class to store the combination of a permission setting and the origin the permission is
     * relevant for.
     *
     * It would make more sense for this to be a subclass of
     * {@link TrustedWebActivityPermissionManager} or a top level class. Unfortunately for the JNI
     * tool to be able to handle passing a class over the JNI boundary the class either needs to be
     * in this file or imported explicitly. Our presubmits don't like explicitly importing classes
     * that we don't need to, so it's easier to just let the class live here.
     */
    static class Permission {
        public final Origin origin;
        public final @ContentSettingValues int setting;

        public Permission(Origin origin, @ContentSettingValues int setting) {
            this.origin = origin;
            this.setting = setting;
        }
    }

    public static void notifyPermissionsChange(@ContentSettingsType int type) {
        if (sNativeInstalledWebappProvider == 0) return;

        InstalledWebappBridgeJni.get().notifyPermissionsChange(
                sNativeInstalledWebappProvider, type);
    }

    public static void onGetPermissionResult(long callback, boolean allow) {
        if (callback == 0) return;

        InstalledWebappBridgeJni.get().notifyPermissionResult(callback, allow);
    }

    @CalledByNative
    private static void setInstalledWebappProvider(long provider) {
        sNativeInstalledWebappProvider = provider;
    }

    @CalledByNative
    private static Permission[] getPermissions(@ContentSettingsType int type) {
        return TrustedWebActivityPermissionManager.get().getPermissions(type);
    }

    @CalledByNative
    private static String getOriginFromPermission(Permission permission) {
        return permission.origin.toString();
    }

    @CalledByNative
    private static int getSettingFromPermission(Permission permission) {
        return permission.setting;
    }

    @CalledByNative
    private static void decidePermission(String url, long callback) {
        Origin origin = Origin.create(Uri.parse(url));
        if (origin == null) {
            onGetPermissionResult(callback, false);
            return;
        }
        PermissionUpdater.get().getLocationPermission(origin, callback);
    }

    @NativeMethods
    interface Natives {
        void notifyPermissionsChange(long provider, int type);
        void notifyPermissionResult(long callback, boolean allow);
    }
}
