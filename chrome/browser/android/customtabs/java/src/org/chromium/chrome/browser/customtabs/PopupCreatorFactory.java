// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Factory for {@link PopupCreator}. This class is in a low-level package to allow modularized
 * components to access the singleton without depending on the full app implementation.
 *
 * <p>The implementation should be registered by the app layer at startup using {@link
 * #setInstance(PopupCreator)}.
 */
@NullMarked
public class PopupCreatorFactory {
    private static @Nullable PopupCreator sInstance;

    protected PopupCreatorFactory() {}

    /** Returns the {@link PopupCreator} instance. */
    public static PopupCreator getInstance() {
        if (sInstance != null) return sInstance;
        throw new IllegalStateException("PopupCreator has not been registered.");
    }

    /**
     * Registers the {@link PopupCreator} instance.
     *
     * @param creator The instance to register.
     */
    public static void setInstance(PopupCreator creator) {
        sInstance = creator;
    }

    public static void setInstanceForTesting(PopupCreator creator) {
        sInstance = creator;
        ResettersForTesting.register(() -> sInstance = null);
    }
}
