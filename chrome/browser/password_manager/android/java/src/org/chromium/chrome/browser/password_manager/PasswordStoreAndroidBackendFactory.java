// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This factory returns an implementation for the backend. The factory itself is implemented
 * downstream, too.
 */
@NullMarked
public abstract class PasswordStoreAndroidBackendFactory {
    private static @Nullable PasswordStoreAndroidBackendFactory sInstance;

    /**
     * Returns a backend factory to be invoked whenever {@link #createBackend()} is called. If no
     * factory was used yet, it is created.
     *
     * @return The shared {@link PasswordStoreAndroidBackendFactory} instance.
     */
    public static PasswordStoreAndroidBackendFactory getInstance() {
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(PasswordStoreAndroidBackendFactory.class);
        }
        if (sInstance == null) {
            sInstance = new PasswordStoreAndroidBackendFactoryUpstreamImpl();
        }
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return A non-null implementation of the {@link PasswordStoreAndroidBackend}.
     */
    public @Nullable PasswordStoreAndroidBackend createBackend() {
        return null;
    }

    public static void setFactoryInstanceForTesting(
            @Nullable PasswordStoreAndroidBackendFactory factory) {
        var oldValue = sInstance;
        sInstance = factory;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }
}
