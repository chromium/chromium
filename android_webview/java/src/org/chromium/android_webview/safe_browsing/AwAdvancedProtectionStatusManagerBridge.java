// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_browsing;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;

import androidx.core.content.ContextCompat;
import androidx.core.os.BuildCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.safe_browsing.OsAdditionalSecurityProvider;
import org.chromium.components.safe_browsing.OsAdditionalSecurityUtil;

/** Observes Android-OS-provided advanced protection state. */
@JNINamespace("android_webview")
@NullMarked
public class AwAdvancedProtectionStatusManagerBridge {
    private static OsAdditionalSecurityProvider.@Nullable Observer sObserver;

    private static class LazyHolder {
        static final boolean sCanQueryAdvancedProtectionPermission =
                canQueryAdvancedProtectionPermission();
    }

    @CalledByNative
    public static void startObserving() {
        if (sObserver != null || !LazyHolder.sCanQueryAdvancedProtectionPermission) return;

        var provider = OsAdditionalSecurityUtil.getProviderInstance();

        sObserver =
                () ->
                        AwAdvancedProtectionStatusManagerBridgeJni.get()
                                .onAdvancedProtectionOsSettingChanged();
        provider.addObserver(sObserver);
    }

    @CalledByNative
    public static void stopObserving() {
        if (sObserver == null) return;

        var provider = OsAdditionalSecurityUtil.getProviderInstance();
        provider.removeObserver(sObserver);
        sObserver = null;
    }

    @CalledByNative
    public static boolean isUnderAdvancedProtection() {
        if (!LazyHolder.sCanQueryAdvancedProtectionPermission) return false;
        var provider = OsAdditionalSecurityUtil.getProviderInstance();
        return provider != null && provider.isAdvancedProtectionRequestedByOs();
    }

    private static boolean canQueryAdvancedProtectionPermission() {
        if (!BuildCompat.isAtLeastB()) {
            return false;
        }
        Context context = ContextUtils.getApplicationContext();
        return ContextCompat.checkSelfPermission(
                        context, Manifest.permission.QUERY_ADVANCED_PROTECTION_MODE)
                == PackageManager.PERMISSION_GRANTED;
    }

    @NativeMethods
    interface Natives {
        void onAdvancedProtectionOsSettingChanged();
    }
}
