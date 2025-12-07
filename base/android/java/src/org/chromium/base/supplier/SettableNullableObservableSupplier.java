// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A supplier that is settable.
 *
 * @param <T> The type of the object supplied.
 */
@NullMarked
// TODO(agrieve): Add @DoNotMock
public interface SettableNullableObservableSupplier<T>
        extends NullableObservableSupplier<T>, Destroyable {
    /**
     * Sets the value of the supplier.
     *
     * @param value The new value.
     */
    void set(@Nullable T value);

    /** Clears observers and sets value to null. */
    @Override
    void destroy();
}
