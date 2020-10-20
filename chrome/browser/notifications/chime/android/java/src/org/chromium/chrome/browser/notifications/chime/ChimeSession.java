// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.chime;

import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.modules.chime.ChimeModule;

/**
 * Used to register to Chime notification platform.
 */
public class ChimeSession {
    private static boolean sRegistered;

    /**
     * Initializes the Chime component. If the DFM is not installed or the feature flag is not
     * enabled, do nothing.
     */
    public static void init() {
        // Don't init if we don't install DFM yet or the feature is not enabled. The DFM install
        // happens during the first time registration.
        if (!ChimeModule.isInstalled() || !isEnabled()) return;

        ChimeModule.getImpl().initialize();
    }

    /**
     * Registers to Chime and start to receive notifications. Internally it will install the DFM
     * first.
     */
    public static void register() {
        if (!isEnabled() || sRegistered) return;

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

    private static boolean isEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.USE_CHIME_ANDROID_SDK);
    }
}
