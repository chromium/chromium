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
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.util.XrUtils;

/** Unit Tests for {@link TabUiFeatureUtilities}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiFeatureUtilitiesUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private void setAccessibilityEnabledForTesting(Boolean value) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(value));
    }

    @Before
    public void setUp() {
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
    @DisableFeatures(ChromeFeatureList.DISABLE_LIST_TAB_SWITCHER)
    public void testCacheGridTabSwitcher_LowEnd() {
        assertTrue(TabUiFeatureUtilities.shouldUseListMode());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();

        assertTrue(TabUiFeatureUtilities.shouldUseListMode());
    }

    @Test
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    @EnableFeatures(ChromeFeatureList.DISABLE_LIST_TAB_SWITCHER)
    public void testCacheGridTabSwitcher_LowEnd_ListDisabled() {
        assertFalse(TabUiFeatureUtilities.shouldUseListMode());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();

        assertFalse(TabUiFeatureUtilities.shouldUseListMode());
    }

    @Test
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FULLSCREEN})
    public void testDisableFullScreen_XrDeviceWithFlag_isTrue() {
        XrUtils.setXrDeviceForTesting(true);

        assertFalse(DeviceClassManager.enableFullscreen());
    }

    @Test
    public void testDisableFullScreen_XrDeviceWithoutFlag_isTrue() {
        XrUtils.setXrDeviceForTesting(true);

        assertFalse(DeviceClassManager.enableFullscreen());
    }
}
