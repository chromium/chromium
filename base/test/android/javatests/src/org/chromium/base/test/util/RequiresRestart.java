// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Overrides a {@link Batch} annotation on a class for a particular test method. This specific test
 * method will run in a fresh process while the rest of the class still runs in whatever batch is
 * declared by the {@link Batch} annotation.
 *
 * <p>This can only be applied to individual test methods. If you would like to indicate that every
 * method in the class needs a fresh process, consider using {@link DoNotBatch} on the class.
 *
 * <p>Optionally supply a message explaining why restart is needed as the value.
 */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface RequiresRestart {
    String value() default "";
}
