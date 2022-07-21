// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import androidx.annotation.VisibleForTesting;

/**
 * This factory returns an implementation for the launcher. The factory itself is also implemented
 * downstream.
 */
public abstract class CredentialManagerLauncherFactory {
    private static CredentialManagerLauncherFactory sInstance;

    /**
     * Returns a launcher factory to be invoked whenever {@link #createLauncher()} is called. If no
     * factory was used yet, it is created.
     *
     * @return The shared {@link CredentialManagerLauncherFactory} instance.
     */
    public static CredentialManagerLauncherFactory getInstance() {
        assertOnUiThread();
        if (sInstance == null) sInstance = new CredentialManagerLauncherFactoryImpl();
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link CredentialManagerLauncher} if one exists.
     *
     * TODO(crbug.com/1346239): Check if backend could be instantiated and throw error
     */
    public CredentialManagerLauncher createLauncher()
            throws CredentialManagerLauncher.CredentialManagerBackendException {
        return null;
    }

    @VisibleForTesting
    public static void setFactoryForTesting(
            CredentialManagerLauncherFactory credentialManagerLauncherFactory) {
        sInstance = credentialManagerLauncherFactory;
    }
}
