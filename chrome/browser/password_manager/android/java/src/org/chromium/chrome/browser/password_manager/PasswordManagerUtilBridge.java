// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.pm.PackageInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.PackageUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;

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
    public static boolean isPasswordManagerAvailable(PrefService prefService) {
        return PasswordManagerUtilBridgeJni.get()
                .isPasswordManagerAvailable(prefService, isInternalBackendPresent());
    }

    /**
     * Checks if the GMSCore update is required to use the Password Manager functionality.
     *
     * @param syncService The sync service.
     * @return Whether the user is required to update GMSCore to use the Password Manager
     *     functionality.
     */
    public static boolean isGmsCoreUpdateRequired(@Nullable SyncService syncService) {
        return PasswordManagerUtilBridgeJni.get().isGmsCoreUpdateRequired(syncService);
    }

    @CalledByNative
    public static boolean isInternalBackendPresent() {
        return PasswordManagerBackendSupportHelper.getInstance().isBackendPresent();
    }

    @CalledByNative
    public static boolean isPlayStoreAppPresent() {
        PackageInfo packageInfo = PackageUtils.getPackageInfo("com.android.vending", 0);
        return packageInfo != null;
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
                && PasswordManagerUtilBridge.isPlayStoreAppPresent();
    }

    @NativeMethods
    public interface Natives {
        boolean isPasswordManagerAvailable(
                @JniType("PrefService*") PrefService prefService, boolean isInternalBackendPresent);

        boolean isGmsCoreUpdateRequired(
                @JniType("syncer::SyncService*") @Nullable SyncService syncService);
    }
}
