// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.data;

import org.chromium.base.supplier.Supplier;

/**
 * Adapter for providers that should only forward provided data if a given propagation condition is
 * met (e.g. if a browser tab is currently active). If the condition isn't met, the provided data is
 * dropped.
 * @param <T> The type of data the provider forwards conditionally.
 * @see CachedProviderAdapter for a conditional provider adapter that can postpone the notification
 *      about provided data by caching it and trigger it later.
 */
public class ConditionalProviderAdapter<T> extends PropertyProvider<T>
        implements Provider.Observer<T> {
    private final Supplier<Boolean> mPropagationCondition;

    public ConditionalProviderAdapter(
            PropertyProvider<T> provider, Supplier<Boolean> propagationCondition) {
        mPropagationCondition = propagationCondition;
        provider.addObserver(this);
    }

    @Override
    public void onItemAvailable(int typeId, T item) {
        if (mPropagationCondition.get()) notifyObservers(item);
    }
}
