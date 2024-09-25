// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;

/**
 * This factory returns an implementation for the helper. The factory itself is also implemented
 * downstream.
 */
public abstract class PasswordCheckupClientHelperFactory {
    private static PasswordCheckupClientHelperFactory sInstance;

    /**
     * Return an instance of PasswordCheckupClientHelperFactory. If no factory was used yet, it is
     * created.
     */
    public static PasswordCheckupClientHelperFactory getInstance() {
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(PasswordCheckupClientHelperFactory.class);
        }
        if (sInstance == null) {
            sInstance = new PasswordCheckupClientHelperFactoryUpstreamImpl();
        }
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordCheckupClientHelper} if one exists.
     *     <p>TODO(crbug.com/40854052): Check if backend could be instantiated and throw error
     */
    public PasswordCheckupClientHelper createHelper() throws PasswordCheckBackendException {
        return null;
    }

    /**
     * Creates and returns new instance of the downstream implementation provided by subclasses.
     *
     * Downstream should override this method with actual implementation.
     *
     * @return An implementation of the {@link PasswordCheckupClientHelper} if one exists.
     */
    protected PasswordCheckupClientHelper doCreateHelper(Context context)
            throws PasswordCheckBackendException {
        throw new PasswordCheckBackendException(
                "Downstream implementation is not present.",
                CredentialManagerError.BACKEND_NOT_AVAILABLE);
    }

    public static void setFactoryForTesting(
            PasswordCheckupClientHelperFactory passwordCheckupClientHelperFactory) {
        var oldValue = sInstance;
        sInstance = passwordCheckupClientHelperFactory;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }
}
