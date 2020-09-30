// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.util.ArrayMap;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.Map;

/**
 * Tests the behavior of {@link ChromeFeatureList} in instrumentation tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeFeatureListInstrumentationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    public void testNoOverridesDefault() {
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TEST_DEFAULT_DISABLED)
    @DisableFeatures(ChromeFeatureList.TEST_DEFAULT_ENABLED)
    public void testAnnotations() {
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));
    }

    @Test
    @MediumTest
    public void testSetTestFeatures() {
        Map<String, Boolean> overrides = new ArrayMap<>();
        overrides.put(ChromeFeatureList.TEST_DEFAULT_DISABLED, true);
        overrides.put(ChromeFeatureList.TEST_DEFAULT_ENABLED, false);
        ChromeFeatureList.setTestFeatures(overrides);

        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));

        ChromeFeatureList.setTestFeatures(null);
    }
}
