// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit Tests for {@link TabUiFeatureUtilities}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiFeatureUtilitiesUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private void setAccessibilityEnabledForTesting(Boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(value));
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        setAccessibilityEnabledForTesting(false);
    }

    @After
    public void tearDown() {
        setAccessibilityEnabledForTesting(null);
        DeviceClassManager.resetForTesting();
    }

    @Test
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd() {
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();

        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd() {
        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();

        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }
}
