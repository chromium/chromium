// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Indicates this class should not be batched and all test methods should run in a fresh process.
 * Using this annotation indicates that the Instrumentation Runner (and hence the process) need to
 * be restarted between these tests. This is the opposite of {@link Batch}.
 *
 * <p>This can only be applied to the entire class. If you would like to indicate that only some
 * methods in the class need a fresh process, consider using {@link RequiresRestart} on those
 * methods and applying a {@link Batch} annotation on the test class.
 *
 * <p>Supply a message explaining why batching is not appropriate.
 *
 * <p>This annotation is the same as the default behavior (when neither DoNotBatch nor Batch are
 * applied), however it's preferable to explicitly apply this annotation if you know there are
 * obstacles to batching a particular test class.
 */
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
public @interface DoNotBatch {
    String reason();
}
