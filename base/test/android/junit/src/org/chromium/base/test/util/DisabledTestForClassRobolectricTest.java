// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJvmTestRunner;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the DisabledTest annotation in Robolectric tests. */
@RunWith(BaseRobolectricTestRunner.class)
@DisabledTest(message = "This test suite should be disabled")
public class DisabledTestForClassRobolectricTest {
    @Test
    public void testTestsInDisabledSuitesAreNotExecuted() {
        Assert.assertTrue(
                "Tests suites marked with @DisabledTest annotation should not be executed!",
                BaseJvmTestRunner.sRunDisabled);
    }
}
