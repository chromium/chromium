// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helpers related to NullAway's nullness checking. */
@NullMarked
public class NullUtil {
    private NullUtil() {}

    /**
     * Tell NullAway that the given parameter is non-null, and asserting to raise a runtime error if
     * the parameter is actually null. Using the return value is optional.
     *
     * <p>Prefer "assumeNotNull(foo);" over "assert foo != null;" when "foo" will be immediately
     * dereferenced (since dereferencing checks for nullness anyways).
     *
     * <p>For when a runtime check is preferred on all channels (since asserts only fire on canary),
     * use Objects.requireNonNull().
     *
     * <p>Expressions are supported. E.g.: assertNonNull(foo.getBar());
     */
    @SuppressWarnings("NullAway") // Since it does not actually check.
    @Contract("null -> fail") // Means you do not need to use the return value.
    public static <T> T assertNonNull(@Nullable T object) {
        assert object != null;
        return object;
    }

    /**
     * Tell NullAway that the given parameter is non-null. Using the return value is optional.
     *
     * <p>Prefer "assumeNotNull(foo);" over "assertNotNull(foo);" when "foo" will be immediately
     * dereferenced (since dereferencing checks for nullness anyways).
     *
     * <p>When foo is not immediately dereferenced, prefer assertNonNull(). When a runtime check is
     * preferred on all channels (since asserts only fire on canary), use Objects.requireNonNull().
     *
     * <p>Expressions are supported. E.g.: assumeNonNull(foo.getBar());
     */
    @SuppressWarnings("NullAway") // Since it does not actually check.
    @Contract("null -> fail") // Means you do not need to use the return value.
    public static <T> T assumeNonNull(@Nullable T object) {
        return object;
    }
}
