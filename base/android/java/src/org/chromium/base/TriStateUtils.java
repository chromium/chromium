// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utility methods for converting between boolean, {@link Boolean}, and {@link TriState}. */
@NullMarked
public class TriStateUtils {
    private TriStateUtils() {}

    /**
     * Converts a primitive boolean to a @TriState int.
     *
     * @param value The primitive boolean value.
     * @return {@link TriState#TRUE} if true, {@link TriState#FALSE} if false.
     */
    public static @TriState int from(boolean value) {
        return value ? TriState.TRUE : TriState.FALSE;
    }

    // TODO(crbug.com/545986483): Remove this method upon migration.
    /**
     * Converts a @Nullable Boolean to a @TriState int.
     *
     * @param value The boxed Boolean value, or null.
     * @return {@link TriState#NOT_SET} if null, {@link TriState#TRUE} if TRUE, {@link
     *     TriState#FALSE} if FALSE.
     */
    public static @TriState int from(@Nullable Boolean value) {
        if (value == null) {
            return TriState.NOT_SET;
        }
        return from(value.booleanValue());
    }

    // TODO(crbug.com/545986483): Remove this method upon migration.
    /**
     * Converts a @TriState int to a @Nullable Boolean.
     *
     * @param value The @TriState int.
     * @return null if {@link TriState#NOT_SET}, Boolean.TRUE if {@link TriState#TRUE},
     *     Boolean.FALSE if {@link TriState#FALSE}.
     */
    public static @Nullable Boolean toNullableBoolean(@TriState int value) {
        return switch (value) {
            case TriState.TRUE -> Boolean.TRUE;
            case TriState.FALSE -> Boolean.FALSE;
            default -> null;
        };
    }
}
