// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

/**
 * This factory returns an implementation for the {@link PasswordSyncControllerDelegate}.
 * The factory itself is implemented downstream, too.
 */
public abstract class PasswordSyncControllerDelegateFactory {
    private static PasswordSyncControllerDelegateFactory sInstance;

    /**
     * Returns a delegate factory to be invoked whenever {@link #createDelegate()} is called. If no
     * factory was used yet, it is created.
     *
     * @return The shared {@link PasswordSyncControllerDelegateFactory} instance.
     */
    public static PasswordSyncControllerDelegateFactory getInstance() {
        assertOnUiThread();
        if (sInstance == null) sInstance = new PasswordSyncControllerDelegateFactoryImpl();
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordSyncControllerDelegate}. May be
     * null for builds without a downstream delegate implementation.
     */
    public PasswordSyncControllerDelegate createDelegate() {
        return null;
    }

    @VisibleForTesting
    public static void setFactoryInstanceForTesting(
            @Nullable PasswordSyncControllerDelegateFactory factory) {
        sInstance = factory;
    }
}
