// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.chime;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.modules.chime.ChimeModule;

/**
 * Used to register to Chime notification platform.
 */
public class ChimeSession {
    private static boolean sRegistered;

    /**
     * Registers to Chime and start to receive notifications.
     */
    public static void start() {
        // TODO(xingliu): Find a better way to access feature in Java code.
        // https://crbug.com/1017860.
        if (!ChimeSessionJni.get().isEnabled() || sRegistered) return;

        // Install the DFM and then reigster.
        if (ChimeModule.isInstalled()) {
            registerChimeInternal();
            return;
        }

        ChimeModule.install((success) -> {
            if (success) registerChimeInternal();
        });
    }

    private static void registerChimeInternal() {
        assert (ChimeModule.isInstalled());
        sRegistered = true;
        ChimeModule.getImpl().register();
    }

    @NativeMethods
    interface Natives {
        /**
         * @return Whether Chime is enabled.
         */
        boolean isEnabled();
    }
}
