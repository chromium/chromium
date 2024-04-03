// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.net.Uri;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

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
     * {@link InstalledWebappPermissionManager} or a top level class. Unfortunately for the JNI
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

    public static void notifyPermissionsChange(@ContentSettingsType.EnumType int type) {
        if (sNativeInstalledWebappProvider == 0) return;

        InstalledWebappBridgeJni.get()
                .notifyPermissionsChange(sNativeInstalledWebappProvider, type);
    }

    public static void runPermissionCallback(
            long callback, @ContentSettingValues int settingValue) {
        if (callback == 0) return;

        InstalledWebappBridgeJni.get().runPermissionCallback(callback, settingValue);
    }

    @CalledByNative
    private static void setInstalledWebappProvider(long provider) {
        sNativeInstalledWebappProvider = provider;
    }

    @CalledByNative
    private static Permission[] getPermissions(@ContentSettingsType.EnumType int type) {
        return InstalledWebappPermissionManager.get().getPermissions(type);
    }

    @CalledByNative
    private static @JniType("std::string") String getOriginFromPermission(Permission permission) {
        return permission.origin.toString();
    }

    @CalledByNative
    private static int getSettingFromPermission(Permission permission) {
        return permission.setting;
    }

    @CalledByNative
    private static void decidePermission(
            @ContentSettingsType.EnumType int type,
            @JniType("std::string") String originUrl,
            @JniType("std::string") String lastCommittedUrl,
            long callback) {
        Origin origin = Origin.create(Uri.parse(originUrl));
        if (origin == null) {
            runPermissionCallback(callback, ContentSettingValues.BLOCK);
            return;
        }
        switch (type) {
            case ContentSettingsType.GEOLOCATION:
                PermissionUpdater.get().getLocationPermission(origin, lastCommittedUrl, callback);
                break;
            case ContentSettingsType.NOTIFICATIONS:
                PermissionUpdater.get()
                        .requestNotificationPermission(origin, lastCommittedUrl, callback);
                break;
            default:
                throw new IllegalStateException("Unsupported permission type.");
        }
    }

    @NativeMethods
    interface Natives {
        void notifyPermissionsChange(long provider, int type);

        void runPermissionCallback(long callback, @ContentSettingValues int settingValue);
    }
}
