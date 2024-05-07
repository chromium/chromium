// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

/**
 * Based on Java 8's java.util.function.Supplier.
 * Similar to Callable<T>, but without a checked Exception and with #hasValue().
 *
 * @param <T> Return type.
 */
public interface Supplier<T> extends java.util.function.Supplier<T> {
    /** Returns whether the supplier holds a value currently. */
    default boolean hasValue() {
        return get() != null;
    }
}
