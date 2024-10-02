// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.UserManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.enterprise.EnterpriseState;
import org.chromium.base.ContextUtils;

/** Utility class to determine if the app is running in an enterprise-owned context. */
@Lifetime.WebView
@JNINamespace("android_webview::enterprise")
public class AwEnterpriseHelper {

    /**
     * Attempt to determine if the app is running on a device or in a profile that is enterprise
     * managed. This can not conclusively be determined on SDK < R as the requisite APIs are not
     * available.
     *
     * @return The best guess as to whether the app is running in an enterprise managed context.
     */
    @CalledByNative
    public static @EnterpriseState int getEnterpriseState() {
        Context context = ContextUtils.getApplicationContext();
        final UserManager um = context.getSystemService(UserManager.class);
        if (VERSION.SDK_INT >= VERSION_CODES.R) {
            // There are a few other examples of managed profiles that are not work profiles, but
            // all work profiles are managed profiles.
            boolean workProfile = um.isManagedProfile();

            // From Android R, this restriction is automatically set by the system if a device
            // manager app is installed, i.e. if the device is enterprise owned.
            // The constant is deprecated because the device manager app no longer needs to set it
            // explicitly, but it can still be checked by other apps.
            //noinspection deprecation
            boolean enterpriseDevice =
                    um.hasUserRestriction(UserManager.DISALLOW_ADD_MANAGED_PROFILE);
            if (workProfile || enterpriseDevice) {
                return EnterpriseState.ENTERPRISE_OWNED;
            }
            return EnterpriseState.NOT_OWNED;
        }

        // We are not able to definitively determine if the device/profile is enterprise owned.
        return EnterpriseState.UNKNOWN;
    }
}
