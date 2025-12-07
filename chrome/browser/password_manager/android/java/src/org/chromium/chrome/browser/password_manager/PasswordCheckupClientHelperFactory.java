// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This factory returns an implementation for the helper. The factory itself is also implemented
 * downstream.
 */
@NullMarked
public abstract class PasswordCheckupClientHelperFactory {
    private static @Nullable PasswordCheckupClientHelperFactory sInstance;

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
    public @Nullable PasswordCheckupClientHelper createHelper() {
        return null;
    }

    public static void setFactoryForTesting(
            PasswordCheckupClientHelperFactory passwordCheckupClientHelperFactory) {
        var oldValue = sInstance;
        sInstance = passwordCheckupClientHelperFactory;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }
}
