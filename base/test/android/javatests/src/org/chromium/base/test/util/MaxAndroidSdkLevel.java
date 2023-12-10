// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Skips the annotated test when the device's API level is higher than {@code value()}. */
@Inherited
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD, ElementType.TYPE})
public @interface MaxAndroidSdkLevel {
    /** Maximum API level in which the test is run. */
    int value();

    /** Why a test is skipped above this API level. */
    String reason() default "";
}
