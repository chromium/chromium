// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;

/**
 * ObservableSupplier wraps an asynchronously provided object E, notifying observers when the
 * dependency is available. This allows classes dependent on E to be provided with a
 * ObservableSupplier during construction and register a Callback<E> to be notified when the needed
 * dependency is available.
 *
 * This class must only be accessed from a single thread.
 *
 * For classes owning the ObservableSupplier and providing it as a dependency to others, see
 * {@link ObservableSupplierImpl}.
 *
 * For classes using a ObservableSupplier to receive a dependency:
 *   - To be notified when the object is available, call {@link #addObserver(Callback)} with a
 *     Callback to be notified when the object is available.
 *   - If the object is already available, the Callback will be called immediately.
 *   - The Callback may be called multiple times if the object wrapped by the ObservableSupplier
 *     changes.
 *
 * @param <E> The type of the wrapped object.
 */
public interface ObservableSupplier<E> extends Supplier<E> {
    /**
     * @param obs An observer to be notified when the object owned by this supplier is available.
     *       If the object is already available, the callback will be notified at the end of the
     *       current message loop (so long as the object hasn't changed).
     * @return The current object or null if it hasn't been set yet.
     */
    E addObserver(Callback<E> obs);

    /**
     * @param obs The observer to remove.
     */
    void removeObserver(Callback<E> obs);
}