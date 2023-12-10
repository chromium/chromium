// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Map;

/** Unit Tests for {@link MutableFlagWithSafeDefault}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MutableFlagWithSafeDefaultUnitTest {
    static final String FEATURE_A_NAME = "FeatureA";
    static final String FEATURE_B_NAME = "FeatureB";
    static final Map<String, Boolean> A_ON_B_OFF =
            Map.of(FEATURE_A_NAME, true, FEATURE_B_NAME, false);

    private static class TestFeatureMap extends FeatureMap {
        @Override
        protected long getNativeMap() {
            return 0;
        }
    }

    private final TestFeatureMap mFeatureMap = new TestFeatureMap();

    @After
    public void tearDown() {
        Flag.resetFlagsForTesting();
    }

    @Test(expected = AssertionError.class)
    public void testDuplicateFeatureInMap_throwsException() {
        mFeatureMap.mutableFlagWithSafeDefault(FEATURE_A_NAME, false);
        mFeatureMap.mutableFlagWithSafeDefault(FEATURE_A_NAME, false);
    }

    @Test(expected = AssertionError.class)
    public void testDuplicateFeatureOutsideOfMap_throwsException() {
        mFeatureMap.mutableFlagWithSafeDefault(FEATURE_A_NAME, false);
        new MutableFlagWithSafeDefault(mFeatureMap, FEATURE_A_NAME, false);
    }

    @Test
    public void testNativeInitialized_getsFromChromeFeatureList() {
        MutableFlagWithSafeDefault featureA =
                mFeatureMap.mutableFlagWithSafeDefault(FEATURE_A_NAME, false);
        MutableFlagWithSafeDefault featureB =
                mFeatureMap.mutableFlagWithSafeDefault(FEATURE_B_NAME, true);

        // Values from ChromeFeatureList should be used from now on.
        FeatureList.setTestFeatures(A_ON_B_OFF);

        // Verify that {@link MutableFlagWithSafeDefault} returns native values.
        assertTrue(featureA.isEnabled());
        assertFalse(featureB.isEnabled());
    }

    @Test
    public void testNativeNotInitialized_useDefault() {
        MutableFlagWithSafeDefault featureA =
                mFeatureMap.mutableFlagWithSafeDefault(FEATURE_A_NAME, false);
        MutableFlagWithSafeDefault featureB =
                mFeatureMap.mutableFlagWithSafeDefault(FEATURE_B_NAME, true);

        // Query the flags to make sure the default values are returned.
        assertFalse(featureA.isEnabled());
        assertTrue(featureB.isEnabled());
    }

    @Test
    public void testNativeInitializedUsedDefault_getsFromChromeFeatureList() {
        MutableFlagWithSafeDefault featureA =
                mFeatureMap.mutableFlagWithSafeDefault(FEATURE_A_NAME, false);
        MutableFlagWithSafeDefault featureB =
                mFeatureMap.mutableFlagWithSafeDefault(FEATURE_B_NAME, true);

        // Query the flags to make sure the default values are returned.
        assertFalse(featureA.isEnabled());
        assertTrue(featureB.isEnabled());

        // Values from ChromeFeatureList should be used from now on.
        FeatureList.setTestFeatures(A_ON_B_OFF);

        // Verify that {@link MutableFlagWithSafeDefault} returns native values.
        assertTrue(featureA.isEnabled());
        assertFalse(featureB.isEnabled());
    }
}
