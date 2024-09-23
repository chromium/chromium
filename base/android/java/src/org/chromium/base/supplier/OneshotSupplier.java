// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;

/**
 * OneshotSupplier wraps an asynchronously provided, non-null object {@code T}, notifying
 * observers a single time when the dependency becomes available. Note that null is the un-set
 * value; a fulfilled supplier will never have a null underlying value.
 *
 * <p>This allows classes dependent on {@code T} to be provided with a
 * OneshotSupplier during construction and register a {@code Callback<T>} to be notified when the
 * needed dependency is available, but without the need to un-register that Callback upon
 * destruction. Contrast to {@link ObservableSupplier}, which requires un-registration to prevent
 * post-destruction callback invocation because the object can change an arbitrary number of times.
 *
 *
 * <p>This class must only be accessed from a single thread. Unless a particular thread designation
 * is given by the owner of the OneshotSupplier, clients should assume it must only be accessed on
 * the UI thread.
 *
 * <p>If you want to create a supplier, see an implementation in {@link OneshotSupplierImpl}.
 *
 * <p>For classes using a OneshotSupplier to receive a dependency:
 * <ul>
 *    <li>To be notified when the object is available, call {@link #onAvailable(Callback)}.
 *    <li>If the object is already available, the Callback will be posted immediately on the handler
 *    for the thread the supplier was created on.
 *    <li>The object held by this supplier will also be returned at the end of {@link
 *    #onAvailable(Callback)}.
 *    <li>The Callback will be called at most once. It's still
 * recommended to use {@link org.chromium.base.CallbackController} for safety.
 * </ul>
 *
 * @param <T> The type of the wrapped object.
 */
public interface OneshotSupplier<T> extends Supplier<T> {
    /**
     * Add a callback that's called when the object owned by this supplier is available.
     * If the object is already available, the callback will be called at the end of the
     * current message loop.
     *
     * @param callback The callback to be called.
     * @return The value for this supplier if already available. Null otherwise.
     */
    @Nullable
    T onAvailable(Callback<T> callback);

    /**
     * Runs the {@link Callback} synchronously if there is already a value assigned, otherwise, it
     * will add the callback to be notified when a value becomes available.
     *
     * @param callback The callback to be called (either async or sync).
     */
    default void runSyncOrOnAvailable(Callback<T> callback) {
        if (hasValue()) {
            callback.onResult(get());
        } else {
            onAvailable(callback);
        }
    }
}
