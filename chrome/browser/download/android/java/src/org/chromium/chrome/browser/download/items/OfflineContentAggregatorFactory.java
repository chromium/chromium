// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.items;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.offline_items_collection.OfflineContentProvider;

/**
 * Basic factory that creates and returns an {@link OfflineContentProvider} that is attached
 * natively to {@link Profile}.
 */
public class OfflineContentAggregatorFactory {
    // We need only one provider, since OfflineContentAggregator lives in the original profile.
    private static OfflineContentProvider sProvider;

    private OfflineContentAggregatorFactory() {}

    /**
     * Allows tests to push a custom {@link OfflineContentProvider} to be used instead of the one
     * pulled from a {@link Profile}.
     * @param provider The {@link OfflineContentProvider} to return.
     */
    @VisibleForTesting
    public static void setOfflineContentProviderForTests(
            @Nullable OfflineContentProvider provider) {
        sProvider = provider;
    }

    /**
     * Used to get access to the offline content aggregator.
     * @return An {@link OfflineContentProvider} instance representing the offline content
     *         aggregator.
     */
    public static OfflineContentProvider get() {
        if (sProvider == null) {
            sProvider = OfflineContentAggregatorFactoryJni.get().getOfflineContentAggregator();
        }
        return sProvider;
    }

    @NativeMethods
    interface Natives {
        OfflineContentProvider getOfflineContentAggregator();
    }
}
