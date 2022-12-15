// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.VisibleForTesting;

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
            sInstance = new PasswordSettingsAccessorFactoryImpl();
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

    public boolean canCreateAccessor() {
        return false;
    }

    /**
     * Creates and returns new instance of the downstream implementation provided by subclasses.
     *
     * Downstream should override this method with actual implementation.
     *
     * @return An implementation of the {@link PasswordSettingsAccessor} if one exists.
     */
    protected PasswordSettingsAccessor doCreateAccessor() throws BackendException {
        throw new BackendException("Downstream implementation is not present.",
                AndroidBackendErrorType.BACKEND_NOT_AVAILABLE);
    }

    @VisibleForTesting
    public static void setupFactoryForTesting(PasswordSettingsAccessorFactory accessorFactory) {
        sInstance = accessorFactory;
    }
}
