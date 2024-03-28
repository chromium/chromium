// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.prefs.PrefService;

/** Wrapper for utilities in password_manager_util. */
public class PasswordManagerUtilBridge {

    /**
     * There are 2 cases when this check returns true: 1) if the user is using UPM and everything
     * works as expected; 2) if the user is eligible for using UPM, but the GMSCore version is too
     * old and doesn't support UPM.
     *
     * @param isPwdSyncEnabled Whether password syncing is enabled.
     * @param prefService The preference service (used to identify whether the preference for using
     *     UPM for local passwords is set)
     * @return Returns true if UPM wiring should be instantiated.
     */
    public static boolean shouldUseUpmWiring(boolean isPwdSyncEnabled, PrefService prefService) {
        return PasswordManagerUtilBridgeJni.get().shouldUseUpmWiring(isPwdSyncEnabled, prefService);
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
     * @param isPwdSyncEnabled Whether password syncing is enabled
     * @return Whether the user is required to update GMSCore to use the Password Manager
     *     functionality.
     */
    public static boolean isGmsCoreUpdateRequired(
            PrefService prefService, boolean isPwdSyncEnabled) {
        return PasswordManagerUtilBridgeJni.get()
                .isGmsCoreUpdateRequired(prefService, isPwdSyncEnabled);
    }

    @CalledByNative
    public static boolean isInternalBackendPresent() {
        return PasswordManagerBackendSupportHelper.getInstance().isBackendPresent();
    }

    /**
     * Checks whether the UPM with sync only available in GMS Core is active for this client.
     *
     * @return True if UPM with sync only available in GMS Core is active, false otherwise.
     */
    public static boolean isUnifiedPasswordManagerSyncOnlyInGMSCoreEnabled() {
        return PasswordManagerUtilBridgeJni.get()
                .isUnifiedPasswordManagerSyncOnlyInGMSCoreEnabled();
    }

    @NativeMethods
    public interface Natives {
        boolean shouldUseUpmWiring(boolean isPwdSyncEnabled, PrefService prefService);

        boolean usesSplitStoresAndUPMForLocal(PrefService prefService);

        boolean isGmsCoreUpdateRequired(PrefService prefService, boolean isPwdSyncEnabled);

        boolean isUnifiedPasswordManagerSyncOnlyInGMSCoreEnabled();
    }
}
