// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_boundary.util;

import org.jspecify.annotations.NullMarked;

/**
 * Exception thrown when an InvocationHandler fails due to method signature mismatches. This class
 * is created to wrap the <a
 * href="https://docs.oracle.com/javase/8/docs/api/java/lang/reflect/Method.html#invoke-java.lang.Object-java.lang.Object...-">IllegalArgumentException
 * thrown by Method.invoke</a>.
 */
@NullMarked
public class InvocationHandlerMethodMismatchException extends RuntimeException {
    public InvocationHandlerMethodMismatchException(Throwable cause) {
        super(cause);
    }
}
