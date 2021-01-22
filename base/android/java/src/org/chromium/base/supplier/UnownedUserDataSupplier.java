// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.NonNull;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;

/**
 * Handles the combined lifecycle management for {@link UnownedUserData} and
 * {@link DestroyableObservableSupplier}.
 * <p>
 * Classes that hold a reference to to the concrete implementation of this class are also in charge
 * of its lifecycle. {@link #destroy} should be called when the applciation is shutting down. This
 * will detach the {@link UnownedUserDataSupplier}, but it won't destroy the supplied object.
 * <p>
 * A functional example with best practices is defined in {@link UnownedUserDataSupplierTest}.
 *
 * @param <E> The type of the data to be Supplied and stored in UnownedUserData.
 * @see UnownedUserDataHost for more details on ownership and typical usage.
 * @see UnownedUserDataKey for information about the type of key that is required.
 * @see UnownedUserData for the marker interface used for this type of data.
 */
public abstract class UnownedUserDataSupplier<E> extends ObservableSupplierImpl<E>
        implements DestroyableObservableSupplier<E>, UnownedUserData {
    private final UnownedUserDataKey<UnownedUserDataSupplier<E>> mUudKey;
    private final UnownedUserDataHost mHost;
    private boolean mIsDestroyed;

    /**
     * Constructs an UnownedUserDataSupplier.
     *
     * @param uudKey The {@link UnownedUserDataKey}, which is defined in subclasses.
     * @param host The {@link UnownedUserDataHost} which hosts the UnownedUserDataSupplier.
     */
    protected UnownedUserDataSupplier(
            @NonNull UnownedUserDataKey<? extends UnownedUserDataSupplier<E>> uudKey,
            @NonNull UnownedUserDataHost host) {
        mUudKey = (UnownedUserDataKey<UnownedUserDataSupplier<E>>) uudKey;
        mHost = host;
        mIsDestroyed = false;

        mUudKey.attachToHost(mHost, this);
    }

    @Override
    public void destroy() {
        assert !mIsDestroyed;
        mUudKey.detachFromHost(mHost);
        mIsDestroyed = true;
    }
}