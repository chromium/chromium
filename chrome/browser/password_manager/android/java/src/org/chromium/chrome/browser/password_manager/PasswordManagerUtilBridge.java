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
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
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
     * There are 2 cases when this check returns true: 1) if the user is using UPM and everything
     * works as expected; 2) if the user is eligible for using UPM, but the GMSCore version is too
     * old and doesn't support UPM.
     *
     * @param syncService The sync service.
     * @param prefService The preference service (used to identify whether the preference for using
     *     UPM for local passwords is set)
     * @return Returns true if UPM wiring should be instantiated.
     */
    public static boolean shouldUseUpmWiring(SyncService syncService, PrefService prefService) {
        return PasswordManagerUtilBridgeJni.get().shouldUseUpmWiring(syncService, prefService);
    }

    /**
     * Checks whether the UPM for local users is activated for this client. This also means that the
     * single password store has been split in account and local stores.
     *
     * @return True if UPM for local users and the split stores are active, false otherwise.
     */
    public static boolean usesSplitStoresAndUPMForLocal(PrefService prefService) {
        return PasswordManagerUtilBridgeJni.get().usesSplitStoresAndUPMForLocal(prefService);
    }

    /**
     * Checks if the GMSCore update is required to use the Password Manager functionality.
     *
     * @param prefService Preference service for checking if the user is enrolled into UPM.
     * @param syncService The sync service.
     * @return Whether the user is required to update GMSCore to use the Password Manager
     *     functionality.
     */
    public static boolean isGmsCoreUpdateRequired(
            PrefService prefService, @Nullable SyncService syncService) {
        return PasswordManagerUtilBridgeJni.get().isGmsCoreUpdateRequired(prefService, syncService);
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

    /**
     * Returns whether Chrome's internal backend is available and the minimum GMS Core requirements
     * for UPM are met.
     */
    public static boolean areMinUpmRequirementsMet() {
        return PasswordManagerUtilBridgeJni.get().areMinUpmRequirementsMet();
    }

    public static @PasswordAccessLossWarningType int getPasswordAccessLossWarningType(
            PrefService prefService) {
        // The warning should not be shown on builds without UPM.
        if (!isInternalBackendPresent()) {
            return PasswordAccessLossWarningType.NONE;
        }
        return PasswordManagerUtilBridgeJni.get().getPasswordAccessLossWarningType(prefService);
    }

    @NativeMethods
    public interface Natives {
        boolean isPasswordManagerAvailable(
                @JniType("PrefService*") PrefService prefService, boolean isInternalBackendPresent);

        boolean shouldUseUpmWiring(
                @JniType("syncer::SyncService*") SyncService syncService,
                @JniType("PrefService*") PrefService prefService);

        boolean usesSplitStoresAndUPMForLocal(@JniType("PrefService*") PrefService prefService);

        boolean isGmsCoreUpdateRequired(
                @JniType("PrefService*") PrefService prefService,
                @JniType("syncer::SyncService*") @Nullable SyncService syncService);

        boolean areMinUpmRequirementsMet();

        @PasswordAccessLossWarningType
        int getPasswordAccessLossWarningType(@JniType("PrefService*") PrefService prefService);
    }
}
