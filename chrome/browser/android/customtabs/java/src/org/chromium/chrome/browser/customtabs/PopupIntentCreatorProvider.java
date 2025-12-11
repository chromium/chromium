// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Utility class to delegate the creation of Custom Tab popup Intents to an implementation of {@link
 * PopupIntentCreator}.
 *
 * <p>This class acts as a service locator, allowing lower-level modules to request popup creation
 * without depending on the implementation details in higher-level modules (e.g., glue).
 */
@NullMarked
public final class PopupIntentCreatorProvider {
    private PopupIntentCreatorProvider() {}

    private static @Nullable PopupIntentCreator sInstance;

    /**
     * Sets the singleton instance of {@link PopupIntentCreator}. This should be called once during
     * application initialization.
     *
     * @param instance The implementation of {@link PopupIntentCreator}.
     */
    public static void setInstance(PopupIntentCreator instance) {
        assert sInstance == null : "PopupIntentCreatorProvider#setInstance() called multiple times";
        sInstance = instance;
    }

    /**
     * Returns the current {@link PopupIntentCreator} instance.
     *
     * @return The registered instance, or null if not yet initialized.
     */
    public static @Nullable PopupIntentCreator getInstance() {
        return sInstance;
    }

    /**
     * Resets the singleton instance to null. This method should only be used in tests to clean up
     * state between test runs.
     */
    public static void resetInstanceForTesting() {
        sInstance = null;
    }
}
