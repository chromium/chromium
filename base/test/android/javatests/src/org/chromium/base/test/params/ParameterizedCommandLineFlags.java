// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation to parameterize flags via the command-line flags file.
 *
 * For example, the following annotation would run the test 3 times:
 *
 * 1st: No flags.
 * 2nd: --enable-features=one --one-params=p1
 * 3rd: --enable-features=two --two-params=p2
 *
 * <code>
 * @ParameterizedCommandLineFlags({
 *     @Switches(),
 *     @Switches({"enable-features=one", "one-params=p1"}),
 *     @Switches({"enable-features=two", "two-params=p2"})
 * })
 * </code>
 */

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD, ElementType.TYPE})
public @interface ParameterizedCommandLineFlags {
    /**
     * Annotation to set commnad line flags in the command-line flags file
     * for JUnit4 instrumentation tests.
     *
     * E.g. if you add the following annotation to your test class:
     *
     * <code>
     * @ParameterizedCommandLineFlags.Switches({"FLAG_A", "FLAG_B"})
     * public class MyTestClass
     * </code>
     *
     * The test harness would run the test once with with --FLAG_A --FLAG_B.
     *
     * If you want to have the method run multiple times with different sets of
     * parameters, see {@link ParameterizedCommandLineFlags}.
     */
    @Inherited
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD, ElementType.TYPE})
    public @interface Switches {
        String[] value() default {};
    }

    Switches[] value() default {};
}
