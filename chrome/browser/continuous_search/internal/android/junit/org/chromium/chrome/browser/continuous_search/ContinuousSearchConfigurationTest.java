// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Unit tests for {@link ContinuousSearchConfiguration}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchConfigurationTest {
    @After
    public void tearDown() {
        SharedPreferencesManager.getInstance().removeKey(
                ContinuousSearchConfiguration.CONTINUOUS_SEARCH_DISMISSAL_COUNT);
    }

    /**
     * Tests that the dismissal count is reset if the dismissal threshold is set to the special
     * value -1.
     */
    @Test
    public void testResetValueOnIgnoreDismissal() {
        initWithDismissalThreshold("-1");
        ContinuousSearchConfiguration.SHARED_PREFERENCES_MANAGER.writeInt(
                ContinuousSearchConfiguration.CONTINUOUS_SEARCH_DISMISSAL_COUNT, 2);
        Assert.assertEquals(2,
                ContinuousSearchConfiguration.SHARED_PREFERENCES_MANAGER.readInt(
                        ContinuousSearchConfiguration.CONTINUOUS_SEARCH_DISMISSAL_COUNT));

        ContinuousSearchConfiguration.initialize();
        Assert.assertEquals(0,
                ContinuousSearchConfiguration.SHARED_PREFERENCES_MANAGER.readInt(
                        ContinuousSearchConfiguration.CONTINUOUS_SEARCH_DISMISSAL_COUNT));
    }

    /**
     * Tests that the dismissals are ignored if the dismissal threshold is set to the special value
     * -1.
     */
    @Test
    public void testIgnoreDismissals() {
        initWithDismissalThreshold("-1");

        ContinuousSearchConfiguration.initialize();
        Assert.assertFalse(ContinuousSearchConfiguration.isPermanentlyDismissed());

        ContinuousSearchConfiguration.recordDismissed(); // no-op.
        Assert.assertFalse(ContinuousSearchConfiguration.isPermanentlyDismissed());
        Assert.assertEquals(0,
                ContinuousSearchConfiguration.SHARED_PREFERENCES_MANAGER.readInt(
                        ContinuousSearchConfiguration.CONTINUOUS_SEARCH_DISMISSAL_COUNT));
    }

    /**
     * Tests that permanent dismissal happens when reaching the dismissal threshold.
     */
    @Test
    public void testTriggerPermanentDismissal() {
        initWithDismissalThreshold("2");
        ContinuousSearchConfiguration.initialize();
        Assert.assertFalse(ContinuousSearchConfiguration.isPermanentlyDismissed());

        ContinuousSearchConfiguration.recordDismissed(); // 1
        Assert.assertFalse(ContinuousSearchConfiguration.isPermanentlyDismissed());

        ContinuousSearchConfiguration.recordDismissed(); // 2
        Assert.assertTrue(ContinuousSearchConfiguration.isPermanentlyDismissed());

        ContinuousSearchConfiguration.recordDismissed(); // stays 2
        Assert.assertTrue(ContinuousSearchConfiguration.isPermanentlyDismissed());
        Assert.assertEquals(2,
                ContinuousSearchConfiguration.SHARED_PREFERENCES_MANAGER.readInt(
                        ContinuousSearchConfiguration.CONTINUOUS_SEARCH_DISMISSAL_COUNT));
    }

    private void initWithDismissalThreshold(String threshold) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CONTINUOUS_SEARCH, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CONTINUOUS_SEARCH,
                ContinuousSearchConfiguration.PERMANENT_DISMISSAL_THRESHOLD, threshold);
        FeatureList.setTestValues(testValues);
    }
}
