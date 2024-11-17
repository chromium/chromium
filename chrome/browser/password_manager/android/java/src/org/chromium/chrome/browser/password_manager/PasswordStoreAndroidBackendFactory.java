// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.chrome.browser.password_manager.PasswordStoreAndroidBackend.BackendException;

/**
 * This factory returns an implementation for the backend. The factory itself is implemented
 * downstream, too.
 */
public abstract class PasswordStoreAndroidBackendFactory {
    private static PasswordStoreAndroidBackendFactory sInstance;

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
    public PasswordStoreAndroidBackend createBackend() {
        return null;
    }

    /**
     * Creates and returns new instance of the downstream implementation provided by subclasses.
     *
     * <p>Downstream should override this method with actual implementation.
     *
     * @return An implementation of the {@link PasswordStoreAndroidBackend} if one exists.
     */
    protected PasswordStoreAndroidBackend doCreateBackend() throws BackendException {
        throw new BackendException(
                "Downstream implementation is not present.",
                AndroidBackendErrorType.BACKEND_NOT_AVAILABLE);
    }

    public static void setFactoryInstanceForTesting(
            @Nullable PasswordStoreAndroidBackendFactory factory) {
        var oldValue = sInstance;
        sInstance = factory;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }
}
