// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.pm.PackageInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.PackageUtils;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;

/** Wrapper for utilities in password_manager_util. */
public class PasswordManagerUtilBridge {

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
            PrefService prefService, SyncService syncService) {
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
     * Returns whether Chrome's internal backend is available and the minimum GMS Core requirements
     * for UPM are met.
     */
    public static boolean areMinUpmRequirementsMet() {
        return PasswordManagerUtilBridgeJni.get().areMinUpmRequirementsMet();
    }

    public static @PasswordAccessLossWarningType int getPasswordAccessLossWarningType(
            PrefService prefService) {
        return PasswordManagerUtilBridgeJni.get().getPasswordAccessLossWarningType(prefService);
    }

    @NativeMethods
    public interface Natives {
        boolean shouldUseUpmWiring(
                @JniType("syncer::SyncService*") SyncService syncService,
                @JniType("PrefService*") PrefService prefService);

        boolean usesSplitStoresAndUPMForLocal(@JniType("PrefService*") PrefService prefService);

        boolean isGmsCoreUpdateRequired(
                @JniType("PrefService*") PrefService prefService,
                @JniType("syncer::SyncService*") SyncService syncService);

        boolean areMinUpmRequirementsMet();

        @PasswordAccessLossWarningType
        int getPasswordAccessLossWarningType(@JniType("PrefService*") PrefService prefService);
    }
}
