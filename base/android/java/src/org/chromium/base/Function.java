// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/**
 * Based on Java 8's {@link java.util.function.Function}.
 *
 * <p>A function that takes a single argument and returns a value.
 *
 * @param <T> Function input type.
 * @param <RT> Function return type.
 */
public interface Function<T, RT> {
    // TODO(crbug.com/1034012): Remove once min Android API level reaches 24.

    /**
     * Returns the result of applying this function to {@code input}.
     *
     * @param input Function input argument.
     * @return Function result.
     */
    RT apply(T input);
}
