// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/**
 * A simple mutable reference. Often used to pass a supplier from one object that's later to another
 * object that's created earlier.
 *
 * <p>Implements {@link Supplier} and {@link Callback} to allow passing to functions without needing
 * to instantiate new lambdas or method references.
 *
 * @param <T> The type of object held.
 */
@NullMarked
public class Holder<T extends @Nullable Object> implements Supplier<T>, Callback<T> {
    public T value;

    /**
     * @param value The value to start with.
     */
    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/1214
    public Holder(@Nullable T value) {
        this.value = value;
    }

    @Override
    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/1209
    public T get() {
        return value;
    }

    @Override
    public void onResult(T value) {
        this.value = value;
    }
}
