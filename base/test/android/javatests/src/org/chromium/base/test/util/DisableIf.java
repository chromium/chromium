// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Repeatable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotations to support conditional test disabling.
 *
 * <p>These annotations should only be used to disable tests that are temporarily failing in some
 * configurations. If a test should never run at all in some configurations, use {@link
 * Restriction}.
 *
 * <p>When this annotation is specified multiple times, each condition is 'or'ed together. In the
 * following either baz or sdk > 22 will cause the test case to be disabled. <code>
 * \@DisableIf.Build(supported_abis_includes = "baz")
 * \@DisableIf.Build(sdk_is_greater_than = 22)
 * </code> In the following either baz or tablet will cause the test case to be disabled. <code>
 * \@DisableIf.Build(supported_abis_includes = "baz")
 * \@DisableIf.Device(DeviceFormFactor.TABLET)
 * </code> When multiple arguments are specified for a single annotation, each condition is 'and'ed
 * together. In the following both baz and sdk > 22 will need to be true for this test case to be
 * disabled. <code>
 * \@DisableIf.Build(supported_abis_includes = "baz", sdk_is_greater_than = 22)
 * </code>
 */
public class DisableIf {
    /** Containing annotations type to wrap {@link DisableIf.Build}. */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface Builds {
        Build[] value();
    }

    /** Conditional disabling based on {@link android.os.Build}. */
    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    @Repeatable(Builds.class)
    public static @interface Build {
        String message() default "";

        int sdk_is_greater_than() default 0;

        int sdk_is_less_than() default Integer.MAX_VALUE;

        int sdk_equals() default 0;

        String supported_abis_includes() default "";

        String hardware_is() default "";

        String product_name_includes() default "";
    }

    @Target({ElementType.METHOD, ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface Device {
        /**
         * @return A list of disabled types.
         */
        String[] value();
    }

    /* Objects of this type should not be created. */
    private DisableIf() {}
}
