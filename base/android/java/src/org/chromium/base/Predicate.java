// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/**
 * Based on Java 8's {@link java.util.function.Predicate}.
 *
 * <p>A function that takes a single argument and returns a boolean.
 *
 * TODO(crbug.com/1034012): Remove interface once min Android API level reaches 24.
 *
 * @param <T> Function input type.
 */
public interface Predicate<T> {
    /**
     * Evaluates this predicate on the given argument.
     *
     * @param input Predicate input argument.
     * @return Predicate result.
     */
    boolean test(T input);
}
