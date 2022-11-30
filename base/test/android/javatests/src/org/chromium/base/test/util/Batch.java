// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation to indicate that this collection of tests is safe to run in batches, where the
 * Instrumentation Runner (and hence the process) does not need to be restarted between these tests.
 *
 * The value passed to this annotation determines which test suites may be run together in the same
 * batch - batches that share a batch name will run in the same Instrumentation invocation. The
 * default value (empty) means the suite runs as its own batch, and the process is restarted before
 * and after the suite.
 *
 * This makes the tests run significantly faster, but you must be careful not to persist changes to
 * global state that could cause other tests in the batch to fail.
 *
 * @Before/@After will run as expected before/after each test method.
 * @BeforeClass/@AfterClass may be used for one-time initialization across all tests within a single
 * suite. Tests wishing to share one-time initialization across suites in the same batch will need
 * to explicitly coordinate.
 *
 * Tests that are safe to run in batch should have this annotation.
 *
 * Tests should have either {@link Batch} or {@link DoNotBatch} annotation.
 */
@Target(ElementType.TYPE)
@Retention(RetentionPolicy.RUNTIME)
public @interface Batch {

    public String value();

    /**
     * Batch name for test suites that are not safe to run batched across multiple suites. The
     * process will not be restarted before each test within this suite, but will be restarted
     * before and after this suite runs.
     */
    public static final String PER_CLASS = "";

    /**
     * Globally shared name for unit tests that can all be batched together.
     *
     * Unit tests must be careful not to persist any changes to global state, or flakes are likely
     * to occur.
     *
     * An exception to this is loading Chrome's native library (eg. using NativeLibraryTestUtils).
     * Your unit tests must assume that the native library may have already been loaded by another
     * test.
     */
    public static final String UNIT_TESTS = "UnitTests";
}
