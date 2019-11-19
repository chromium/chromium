// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.preferences.website.ContentSettingValues;

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

    public static void notifyPermissionsChange() {
        if (sNativeInstalledWebappProvider == 0) return;

        InstalledWebappBridgeJni.get().notifyPermissionsChange(sNativeInstalledWebappProvider);
    }

    @CalledByNative
    private static void setInstalledWebappProvider(long provider) {
        sNativeInstalledWebappProvider = provider;
    }

    @CalledByNative
    private static Permission[] getNotificationPermissions() {
        return TrustedWebActivityPermissionManager.get().getNotificationPermissions();
    }

    @CalledByNative
    private static String getOriginFromPermission(Permission permission) {
        return permission.origin.toString();
    }

    @CalledByNative
    private static int getSettingFromPermission(Permission permission) {
        return permission.setting;
    }

    @NativeMethods
    interface Natives {
        void notifyPermissionsChange(long provider);
    }
}
