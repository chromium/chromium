// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

public class JavaUtils {
    private JavaUtils() {}

    /**
     * Throw {@code t} as an unchecked exception.
     *
     * <p>Prefer this over wrapping exceptions in RuntimeException with no additional context in
     * order to reduce try/catch blocks and to make stack traces easier to follow.
     *
     * <p>While this might seem iffy, know that CheckedExceptions are not a concept at the bytecode
     * level, and that <a
     * href="https://kotlinlang.org/docs/exceptions.html#checked-exceptions">Kotlin has done away
     * with the concept</a>.
     *
     * <pre>{@code
     * try {
     *     someCode();
     * } catch (IOException e) {
     *     // R8 will be able to remove this try/catch since this makes it a no-op.
     *     JavaUtils.throwUnchecked(e);
     *     // For non-void methods, use an unused "throw" to let the compiler know execution stops.
     *     throw JavaUtils.throwUnchecked(e);
     * }
     * }</pre>
     *
     * <p>See: https://www.mail-archive.com/javaposse@googlegroups.com/msg05984.html
     */
    @SuppressWarnings("unchecked")
    public static <T extends Throwable> RuntimeException throwUnchecked(Throwable t) throws T {
        throw (T) t;
    }
}
