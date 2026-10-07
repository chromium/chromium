// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import android.util.SparseArray;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

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

    /**
     * Collects the Java-only runtime signals from all registered self-service handlers, merged into
     * a single map. Used at notification scheduling time to pass feature-specific signals across
     * JNI to the native tips eligibility logic.
     *
     * @param windowAndroid The current {@link WindowAndroid}, forwarded to each handler.
     * @return A map of signal name to value across all registered handlers, or an empty map if
     *     self-service is disabled or no handler provides signals.
     */
    public static Map<String, Float> collectCustomSignals(WindowAndroid windowAndroid) {
        Map<String, Float> customSignals = new HashMap<>();
        if (!ChromeFeatureList.sTipsSelfService.isEnabled()) {
            return customSignals;
        }
        for (int i = 0; i < sProviders.size(); i++) {
            // Both are non-null by contract (@NullMarked), but guard defensively against
            // implementers outside NullAway's reach so a misbehaving feature is skipped rather
            // than crashing background notification scheduling.
            TipsPromoHandler handler = sProviders.valueAt(i).get();
            if (handler == null) continue;
            Map<String, Float> signals = handler.getCustomSignals(windowAndroid);
            if (signals == null) continue;
            customSignals.putAll(signals);
        }
        return customSignals;
    }

    /** Clears all registered providers. For testing use only. */
    public static void resetForTesting() {
        sProviders.clear();
    }

    private TipsPromoHandlerFactory() {}
}
