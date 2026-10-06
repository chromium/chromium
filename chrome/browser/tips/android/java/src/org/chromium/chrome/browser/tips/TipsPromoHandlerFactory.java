// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import android.util.SparseArray;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Registry and factory responsible for managing and creating {@link TipsPromoHandler} instances for
 * eligible feature tips.
 */
@NullMarked
public class TipsPromoHandlerFactory {
    private static final SparseArray<TipsPromoHandler.Provider> sProviders = new SparseArray<>();

    /**
     * Registers a {@link TipsPromoHandler.Provider} for the given feature type.
     *
     * @param featureType The {@link TipsNotificationsFeatureType} to register a provider for.
     * @param provider The {@link TipsPromoHandler.Provider} that instantiates the handler.
     */
    public static void register(
            @TipsNotificationsFeatureType int featureType, TipsPromoHandler.Provider provider) {
        sProviders.put(featureType, provider);
    }

    /**
     * Retrieves the self-service {@link TipsPromoHandler} for the given feature type if available.
     *
     * @param featureType The {@link TipsNotificationsFeatureType} to create a handler for.
     * @return The instantiated {@link TipsPromoHandler}, or null if self-service is disabled or the
     *     feature type has no registered provider.
     */
    public static @Nullable TipsPromoHandler getHandler(
            @TipsNotificationsFeatureType int featureType) {
        if (!ChromeFeatureList.sTipsSelfService.isEnabled()) {
            return null;
        }
        // The feature type should always exist in the providers map when self-service is enabled.
        // Fall back to null (legacy handler) if unmigrated or not found.
        TipsPromoHandler.Provider provider = sProviders.get(featureType);
        return provider != null ? provider.get() : null;
    }

    /** Clears all registered providers. For testing use only. */
    public static void resetForTesting() {
        sProviders.clear();
    }

    private TipsPromoHandlerFactory() {}
}
