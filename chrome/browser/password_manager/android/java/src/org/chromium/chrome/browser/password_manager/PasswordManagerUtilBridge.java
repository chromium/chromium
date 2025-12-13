// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.PackageUtils;
import org.chromium.build.annotations.NullMarked;

/** Wrapper for utilities in password_manager_util. */
@NullMarked
public class PasswordManagerUtilBridge {

    /**
     * Checks whether all the conditions to communicate with the password storage in GMS Core are
     * met. The password manager functionality (saving/filling/management) is only available if
     * those conditions are met.
     *
     * @return whether password manager functionality is available.
     */
    public static boolean isPasswordManagerAvailable() {
        return PasswordManagerUtilBridgeJni.get()
                .isPasswordManagerAvailable(isInternalBackendPresent());
    }

    @CalledByNative
    public static boolean isInternalBackendPresent() {
        return PasswordManagerBackendSupportHelper.getInstance().isBackendPresent();
    }

    /**
     * Checks whether Google Play Services is installed and whether Play Store is installed so that
     * the user can be redirected to the store to update Google Play Services if needed.
     *
     * @return true if both Google Play Services and Google Play Store are installed.
     */
    @CalledByNative
    public static boolean isGooglePlayServicesUpdatable() {
        return PackageUtils.isPackageInstalled("com.google.android.gms")
                && PackageUtils.getPackageInfo("com.android.vending", 0) != null;
    }

    @NativeMethods
    public interface Natives {
        boolean isPasswordManagerAvailable(boolean isInternalBackendPresent);
    }
}
