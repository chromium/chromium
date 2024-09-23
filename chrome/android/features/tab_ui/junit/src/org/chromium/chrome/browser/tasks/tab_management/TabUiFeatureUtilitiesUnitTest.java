// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;

/** Unit Tests for {@link TabUiFeatureUtilities}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING_ENABLE_OEM)
public class TabUiFeatureUtilitiesUnitTest {

    private void setAccessibilityEnabledForTesting(Boolean value) {
        ThreadUtils.runOnUiThreadBlocking(
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
        assertFalse(TabUiFeatureUtilities.shouldUseListMode());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();

        assertFalse(TabUiFeatureUtilities.shouldUseListMode());
    }

    @Test
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd() {
        assertTrue(TabUiFeatureUtilities.shouldUseListMode());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();

        assertTrue(TabUiFeatureUtilities.shouldUseListMode());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void testIsTabDragAsWindowEnabled() {
        assertTrue(TabUiFeatureUtilities.isTabDragAsWindowEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void testTabDragToCreateInstance_withAllowlistedOEM_FFDisabled() {
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");
        assertTrue(TabUiFeatureUtilities.isTabDragToCreateInstanceSupported());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void testTabDragToCreateInstance_withNonAllowlistedOEM_FFEnabled() {
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "other");
        assertTrue(TabUiFeatureUtilities.isTabDragToCreateInstanceSupported());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void testTabDragToCreateInstance_withNonAllowlistedOEM_FFDisabled() {
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "other");
        assertFalse(TabUiFeatureUtilities.isTabDragToCreateInstanceSupported());
    }
}
