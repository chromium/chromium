// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.build.BuildConfig;

/** Example [instrumentation/on-device] [unit] [batched] test. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ExampleOnDeviceUnitTest {
    @Test
    @MediumTest
    public void testFeatureList() {
        // In is_chrome_branded = true builds, CachedFlag's defaultValueInTests is not applied,
        // which means the LoadNativeEarly CachedFlag has a value of false, which means we will not
        // initialize the feature list early. Hence we skip this test when is_chrome_branded = true.
        Assume.assumeFalse(BuildConfig.IS_CHROME_BRANDED);
        assertTrue(FeatureList.isNativeInitialized());
    }
}
