// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Generate a META-INF/services file that maps the annotated class to the given service class.
 *
 * <p>This is the same as @AutoService, but directly supported in Chrome's build system (does not
 * require an annotation processor).
 */
@Target({ElementType.TYPE})
@Retention(RetentionPolicy.CLASS)
public @interface ServiceImpl {
    /** The service implemented by this class. */
    Class<?> value();
}
