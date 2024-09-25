// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.chrome.browser.password_manager.PasswordStoreAndroidBackend.BackendException;

/**
 * This factory returns an implementation for the password settings accessor. The factory itself is
 * also implemented downstream.
 */
public abstract class PasswordSettingsAccessorFactory {
    private static PasswordSettingsAccessorFactory sInstance;

    protected PasswordSettingsAccessorFactory() {}

    /**
     * Returns an accessor factory to be invoked whenever {@link #createAccessor()} is called. If no
     * factory was used yet, it is created.
     *
     * @return The shared {@link PasswordSettingsAccessorFactory} instance.
     */
    public static PasswordSettingsAccessorFactory getOrCreate() {
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(PasswordSettingsAccessorFactory.class);
        }
        if (sInstance == null) {
            sInstance = new PasswordSettingsAccessorFactoryUpstreamImpl();
        }
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordSettingsAccessor} if one exists.
     */
    public PasswordSettingsAccessor createAccessor() {
        return null;
    }

    /**
     * Creates and returns new instance of the downstream implementation provided by subclasses.
     *
     * <p>Downstream should override this method with actual implementation.
     *
     * @return An implementation of the {@link PasswordSettingsAccessor} if one exists.
     */
    protected PasswordSettingsAccessor doCreateAccessor() throws BackendException {
        throw new BackendException(
                "Downstream implementation is not present.",
                AndroidBackendErrorType.BACKEND_NOT_AVAILABLE);
    }

    public static void setupFactoryForTesting(PasswordSettingsAccessorFactory accessorFactory) {
        var oldValue = sInstance;
        sInstance = accessorFactory;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }
}
