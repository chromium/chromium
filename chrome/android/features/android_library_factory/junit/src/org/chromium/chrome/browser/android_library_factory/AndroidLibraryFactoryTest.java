// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.android_library_factory;

import static org.junit.Assert.assertNotNull;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/**
 * Test that javatests can access package private methods when android_library_factory() is
 * involved. The test succeeds if the compile succeeds.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class AndroidLibraryFactoryTest {
    @Test
    @SmallTest
    public void testAll() {
        assertNotNull(new Factory1().packagePrivateMethod());
        assertNotNull(new Factory2().packagePrivateMethod());
    }
}
