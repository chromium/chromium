// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.VisibleForTesting;

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
        if (sInstance == null) sInstance = new PasswordCheckupClientHelperFactoryImpl();
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordCheckupClientHelper} if one exists.
     *
     * TODO(crbug.com/1346239): Check if backend could be instantiated and throw error
     */
    public PasswordCheckupClientHelper createHelper()
            throws PasswordCheckupClientHelper.PasswordCheckBackendException {
        return null;
    }

    @VisibleForTesting
    public static void setFactoryForTesting(
            PasswordCheckupClientHelperFactory passwordCheckupClientHelperFactory) {
        sInstance = passwordCheckupClientHelperFactory;
    }
}
