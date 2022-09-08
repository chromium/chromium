// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.classpath_order;

import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Test that resources defined in different android_resources() targets but with the same
 * package are accessible.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class ClassPathOrderTest {
    @Test
    @SmallTest
    public void testAll() {
        assertTrue(org.chromium.build.classpath_order.test1.R.integer.a1_dependency_resource >= 0);
        assertTrue(org.chromium.build.classpath_order.test1.R.integer.z1_master_resource >= 0);
        assertTrue(org.chromium.build.classpath_order.test2.R.integer.z2_dependency_resource >= 0);
        assertTrue(org.chromium.build.classpath_order.test2.R.integer.a2_master_resource >= 0);
    }
}
