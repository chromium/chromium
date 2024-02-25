// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.lifetime.Destroyable;

/**
 * UnownedUserDataSupplier handles the combined lifecycle management for {@link UnownedUserData} and
 * {@link DestroyableObservableSupplier}. It can be constructed anywhere but needs to be attached
 * before it's accessible via {@link UnownedUserDataHost}. When destroyed, UnownedUserDataSupplier
 * is detached from all hosts.
 *
 * <p>A functional implementation with best practices is defined in {@link
 * UnownedUserDataSupplierTest}.
 *
 * <p>Classes that hold a reference to to the concrete implementation of this class are also in
 * charge of its lifecycle. {@link #destroy} should be called when the application is shutting down.
 * This will detach the {@link UnownedUserDataSupplier}, but it won't destroy the supplied object.
 *
 * <p>In practice, UnownedUserDataSupplier owners should declare and assign the supplier inline.
 * This allows interop between other supplier implementations as well as use in activity
 * constructors before {@link WindowAndroid} is created. See the example below:
 *
 * <pre>{@code
 * UnownedUserDataSupplier<Foo> mFooSupplier = new FooSupplier();
 * ...
 * // Sometime after WindowAndroid has been created.
 * mFooSupplier.attach(mWindowAndroid.getUnownedUserDataHost());
 * }</pre>
 *
 * @param <E> The type of the data to be Supplied and stored in UnownedUserData.
 * @see UnownedUserDataHost for more details on ownership and typical usage.
 * @see UnownedUserDataKey for information about the type of key that is required.
 * @see UnownedUserData for the marker interface used for this type of data.
 */
public abstract class UnownedUserDataSupplier<E> extends ObservableSupplierImpl<E>
        implements Destroyable, UnownedUserData {
    private final UnownedUserDataKey<UnownedUserDataSupplier<E>> mUudKey;
    private final DestroyChecker mDestroyChecker = new DestroyChecker();

    /**
     * Constructs an UnownedUserDataSupplier.
     * @param uudKey The {@link UnownedUserDataKey}, which is defined in subclasses.
     */
    protected UnownedUserDataSupplier(
            @NonNull UnownedUserDataKey<? extends UnownedUserDataSupplier<E>> uudKey) {
        mUudKey = (UnownedUserDataKey<UnownedUserDataSupplier<E>>) uudKey;
    }

    /**
     * Attach to the specified host.
     * @param host The host to attach the supplier to.
     */
    public void attach(@NonNull UnownedUserDataHost host) {
        mDestroyChecker.checkNotDestroyed();
        mUudKey.attachToHost(host, this);
    }

    @Override
    @CallSuper
    public void destroy() {
        mDestroyChecker.destroy();
        mUudKey.detachFromAllHosts(this);
    }
}
