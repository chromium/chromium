// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import android.support.annotation.Nullable;

import org.chromium.base.Callback;

/**
 * Provides a cache for a given provider. New sets of data will only be cached and not
 * propagated to observers immediately. Instead, a callback is called to when data arrives and
 * the decision to make the new data available happens there.
 * @param <T> The type of data the provider collects and provides.
 */
public class CachedProviderAdapter<T> extends PropertyProvider<T> implements Provider.Observer<T> {
    private final @Nullable Callback<CachedProviderAdapter> mNewCachedDataAvailable;
    private T mLastItems;

    /**
     * Creates an adapter that listens to the given |provider| and stores items provided by
     * it. It will not immediately notify observers but instead call a callback when data arrives.
     * @param provider The {@link Provider} to observe and whose data to cache.
     * @param defaultItems The items to be notified about if the Provider hasn't provided any.
     * @param newCachedDataAvailable Optional callback to be called if new data arrives.
     */
    public CachedProviderAdapter(PropertyProvider<T> provider, T defaultItems,
            @Nullable Callback<CachedProviderAdapter> newCachedDataAvailable) {
        super(provider.mType);
        mNewCachedDataAvailable = newCachedDataAvailable;
        provider.addObserver(this);
        mLastItems = defaultItems;
    }

    /**
     * Calls {@link #onItemAvailable} with the last used items again. If there haven't been
     * any calls, call it with an empty list to avoid putting observers in an undefined
     * state.
     */
    public void notifyAboutCachedItems() {
        notifyObservers(mLastItems);
    }

    @Override
    public void onItemAvailable(int typeId, T actions) {
        mLastItems = actions;
        // Update the contents immediately, if the adapter connects to an active element.
        if (mNewCachedDataAvailable != null) mNewCachedDataAvailable.onResult(this);
    }
}
