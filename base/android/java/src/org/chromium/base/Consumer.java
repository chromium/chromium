// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/**
 * Based on Java 8's {@code java.util.function.Consumer}.
 *
 * <p>An operation that accepts a single input argument and returns no result.
 *
 * @param <T> The type of the input to the operation.
 */
public interface Consumer<T> {
    // TODO(crbug.com/1034012): Remove once min Android API level reaches 24.

    /**
     * Performs this operation on the given argument.
     *
     * @param t The input argument.
     */
    void accept(T t);
}
