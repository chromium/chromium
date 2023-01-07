// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * @deprecated Use "@DisabledTest" instead which has identical behavior.
 *
 * This annotation is for flaky tests.
 * <p>
 * Tests with this annotation will not be run on any of the normal bots.
 * Please note that they might eventually run on a special bot.
 */
@Target({ElementType.METHOD, ElementType.TYPE})
@Retention(RetentionPolicy.RUNTIME)
@Deprecated
public @interface FlakyTest {
    String message() default "";
}
