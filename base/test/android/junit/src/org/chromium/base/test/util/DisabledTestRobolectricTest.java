// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the DisabledTest annotation in Robolectric tests. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DisabledTestRobolectricTest {
    @Test
    @DisabledTest(message = "This test should be disabled")
    public void testDisabledTestAreNotExecuted() {
        Assert.fail("Tests marked with @DisabledTest annotation should not be executed!");
    }
}
