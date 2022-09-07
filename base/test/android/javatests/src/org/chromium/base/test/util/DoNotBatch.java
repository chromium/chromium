// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation to indicate that this collection of tests is not safe to run in batches, where the
 * Instrumentation Runner (and hence the process) does not need to be restarted between these
 * tests.
 *
 * Tests that are not safe to run in batch should have this annotation with reasons.
 *
 * Tests should have either {@link Batch} or {@link DoNotBatch} annotation.
 */
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
public @interface DoNotBatch {
    String reason();
}
