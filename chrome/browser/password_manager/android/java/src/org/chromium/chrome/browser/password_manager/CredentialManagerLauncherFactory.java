// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This factory returns an implementation for the launcher. The factory itself is also implemented
 * downstream.
 */
@NullMarked
public abstract class CredentialManagerLauncherFactory {
    private static @Nullable CredentialManagerLauncherFactory sInstance;

    /**
     * Returns a launcher factory to be invoked whenever {@link #createLauncher()} is called. If no
     * factory was used yet, it is created.
     *
     * @return The shared {@link CredentialManagerLauncherFactory} instance.
     */
    public static CredentialManagerLauncherFactory getInstance() {
        assertOnUiThread();
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(CredentialManagerLauncherFactory.class);
        }
        if (sInstance == null) {
            sInstance = new CredentialManagerLauncherFactoryUpstreamImpl();
        }
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link CredentialManagerLauncher} if one exists.
     *     <p>TODO(crbug.com/40854052): Check if backend could be instantiated and throw error
     */
    public @Nullable CredentialManagerLauncher createLauncher() {
        return null;
    }

    public static void setFactoryForTesting(
            CredentialManagerLauncherFactory credentialManagerLauncherFactory) {
        var oldValue = sInstance;
        sInstance = credentialManagerLauncherFactory;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }
}
