// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;

/**
 * SyncOneshotSupplier wraps an asynchronously provided, non-null object {@code T}, synchronously
 * notifying observers a single time when the dependency becomes available. Note that null is the
 * sentinel value; a fulfilled supplier will never have a null value.
 *
 * <p>See {@link OneshotSupplier} for more details on when this might be useful. The key distinction
 * between the two interfaces is that the callbacks registered to {@link #onAvailable(Callback)} are
 * called synchronously when the object becomes is available. This is critical in some applications
 * where the value might be needed immediately and the {@link Callback} cannot be posted. However,
 * generally prefer {@link OneshotSupplier} if either will work to avoid main thread congestion.
 *
 * <p>This class must only be accessed from a single thread. Unless a particular thread designation
 * is given by the owner of the OneshotSupplier, clients should assume it must only be accessed on
 * the UI thread.
 *
 * <p>If you want to create a supplier, see an implementation in {@link SyncOneshotSupplierImpl}.
 *
 * @param <T> The type of the wrapped object.
 */
public interface SyncOneshotSupplier<T> extends Supplier<T> {
    /**
     * Add a callback that's synchronously called when the object owned by this supplier is
     * available. If the object is already available, the callback will be called immediately.
     *
     * @param callback The callback to be called.
     * @return The value for this supplier if already available. Null otherwise.
     */
    @Nullable
    T onAvailable(Callback<T> callback);
}
