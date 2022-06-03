// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.Nullable;

/**
 * Based on Java 8's java.util.function.Supplier.
 * Similar to Callable<T>, but without a checked Exception and with #hasValue().
 *
 * @param <T> Return type.
 */
public interface Supplier<T> {
    /** Returns the value currently held or <code>null</code> when none is held. */
    @Nullable
    T get();

    /**
     * Returns whether the supplier holds a value currently.
     *
     * This default implementation should only be used with trivial implementation of #get(), where
     * the supplier object is accessed. In case it is created on demand, or the Supplier
     * implementation provides a new one every time, this method must be overridden.
     */
    default boolean hasValue() {
        T t = get();
        assert t == get() : "Value provided by #get() must not change.";
        return t != null;
    }
}
