// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;

/** Misc methods related to build tools. */
@NullMarked
public class NullUtil {
    private NullUtil() {}

    /**
     * Tell NullAway that the given parameter is non-null.
     *
     * <p>Prefer this over "assert foo != null;", since asserts will lead to different crash
     * signatures in Canary vs non-Canary (since asserts enabled only on Canary).
     *
     * <p>For when a runtime check is meritted, use Objects.requireNonNull().
     *
     * <p>See: https://github.com/uber/NullAway/wiki/Suppressing-Warnings#downcasting
     */
    @NullUnmarked
    public static <T> T assumeNonNull(@Nullable T object) {
        return object;
    }
}
