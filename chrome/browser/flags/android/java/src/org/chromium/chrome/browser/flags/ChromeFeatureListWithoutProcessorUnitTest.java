// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.Collections;

/**
 * Tests the behavior of {@link ChromeFeatureList} in Robolectric unit tests when the rule
 * Features.JUnitProcessor is NOT present.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeFeatureListWithoutProcessorUnitTest {
    /** In unit tests, all flags checked must have their value specified. */
    @Test(expected = AssertionError.class)
    public void testNoOverridesDefaultDisabled_asserts() {
        ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED);
    }

    /** In unit tests, all flags checked must have their value specified. */
    @Test(expected = AssertionError.class)
    public void testNoOverridesDefaultEnabled_asserts() {
        ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED);
    }

    /**
     * In unit tests without a Features.JUnitProcessor, the EnableFeatures annotation does not work.
     */
    @Test(expected = AssertionError.class)
    @EnableFeatures(ChromeFeatureList.TEST_DEFAULT_DISABLED)
    public void testAnnotationEnabled_asserts() {
        ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED);
    }

    /**
     * In unit tests without a Features.JUnitProcessor, the DisableFeatures annotation does not
     * work.
     */
    @Test(expected = AssertionError.class)
    @DisableFeatures(ChromeFeatureList.TEST_DEFAULT_ENABLED)
    public void testAnnotationDisabled_asserts() {
        ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED);
    }

    /**
     * In unit tests without a Features.JUnitProcessor, flags may have their value specified by
     * calling {@link FeatureList#setTestFeatures(java.util.Map)}.
     */
    @Test
    public void testSetTestFeaturesEnabled_returnsEnabled() {
        FeatureList.setTestFeatures(
                Collections.singletonMap(ChromeFeatureList.TEST_DEFAULT_DISABLED, true));
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
        FeatureList.setTestFeatures(null);
    }

    /**
     * In unit tests without a Features.JUnitProcessor, flags may have their value specified by
     * calling {@link FeatureList#setTestFeatures(java.util.Map)}.
     */
    @Test
    public void testSetTestFeaturesDisabled_returnsDisabled() {
        FeatureList.setTestFeatures(
                Collections.singletonMap(ChromeFeatureList.TEST_DEFAULT_ENABLED, false));
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));
        FeatureList.setTestFeatures(null);
    }
}
