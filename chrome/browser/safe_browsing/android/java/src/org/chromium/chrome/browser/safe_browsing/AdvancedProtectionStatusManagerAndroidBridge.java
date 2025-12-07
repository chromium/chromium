// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;

/** Observes Android-OS-provided advanced protection state. */
@JNINamespace("safe_browsing")
@NullMarked
public class AdvancedProtectionStatusManagerAndroidBridge
        implements OsAdditionalSecurityPermissionProvider.Observer {
    private final long mNative;
    private final @Nullable OsAdditionalSecurityPermissionProvider mProvider;

    @CalledByNative
    private static AdvancedProtectionStatusManagerAndroidBridge create(
            long nativeAdvancedProtectionStatusManagerAndroid) {
        return new AdvancedProtectionStatusManagerAndroidBridge(
                nativeAdvancedProtectionStatusManagerAndroid);
    }

    @CalledByNative
    public static boolean isUnderAdvancedProtection() {
        if (CommandLine.getInstance()
                .hasSwitch(SafeBrowsingSwitches.FORCE_TREAT_USER_AS_ADVANCED_PROTECTION)) {
            return true;
        }

        // Operating-system-requested advanced-protection is the only advanced-protection type
        // currently supported on Android.
        var provider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        return provider != null && provider.isAdvancedProtectionRequestedByOs();
    }

    public AdvancedProtectionStatusManagerAndroidBridge(
            long nativeAdvancedProtectionStatusManagerAndroid) {
        mNative = nativeAdvancedProtectionStatusManagerAndroid;
        mProvider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        if (mProvider == null) return;

        mProvider.addObserver(this);
    }

    @CalledByNative
    private void destroy() {
        if (mProvider == null) return;

        mProvider.removeObserver(this);
    }

    @Override
    public void onAdvancedProtectionOsSettingChanged() {
        AdvancedProtectionStatusManagerAndroidBridgeJni.get()
                .onAdvancedProtectionOsSettingChanged(mNative);
    }

    @NativeMethods
    interface Natives {
        void onAdvancedProtectionOsSettingChanged(
                long nativeAdvancedProtectionStatusManagerAndroid);
    }
}
