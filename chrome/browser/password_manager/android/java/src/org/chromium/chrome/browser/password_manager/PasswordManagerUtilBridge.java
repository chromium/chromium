// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.NativeMethods;

import org.chromium.components.prefs.PrefService;

/** Wrapper for utilities in password_manager_util. */
public class PasswordManagerUtilBridge {
    public static boolean canUseUPMBackend(boolean isPwdSyncEnabled, PrefService prefService) {
        return PasswordManagerUtilBridgeJni.get().canUseUPMBackend(isPwdSyncEnabled, prefService);
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

    @NativeMethods
    public interface Natives {
        boolean canUseUPMBackend(boolean isPwdSyncEnabled, PrefService prefService);

        boolean usesSplitStoresAndUPMForLocal(PrefService prefService);
    }
}
