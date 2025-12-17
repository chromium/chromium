// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/** Test that we can initialize feature list early using the FORCE_INIT_FEATURE_LIST_EARLY flag */
// TODO(469477255): Come up with a way to test initializing feature list early more thoroughly
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.FORCE_INIT_FEATURE_LIST_EARLY})
public class InitFeatureListEarlyTest {
    @Test
    @MediumTest
    public void testFeatureListInitialized() {
        assertTrue(FeatureList.isNativeInitialized());
    }
}
