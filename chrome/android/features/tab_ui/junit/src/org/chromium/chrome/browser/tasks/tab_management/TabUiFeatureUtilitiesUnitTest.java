// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.List;

/**
 * Unit Tests for {@link TabUiFeatureUtilities}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiFeatureUtilitiesUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    CommandLine mCommandLine;

    private void setAccessibilityEnabledForTesting(Boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(value));
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mCommandLine.isNativeImplementation()).thenReturn(true);
        CommandLine.setInstanceForTesting(mCommandLine);

        setAccessibilityEnabledForTesting(false);
        CachedFeatureFlags.resetFlagsForTesting();
    }

    @After
    public void tearDown() {
        CommandLine.reset();
        CachedFeatureFlags.resetFlagsForTesting();
        setAccessibilityEnabledForTesting(null);
        DeviceClassManager.resetForTesting();
        SysUtils.resetForTesting();
    }

    private void cacheFeatureFlags() {
        List<String> featuresToCache = Arrays.asList(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                ChromeFeatureList.TAB_GROUPS_ANDROID,
                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID);
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);
    }

    @Test
    // clang-format off
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_NoEnabledFlags() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_Layout() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_Layout() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_LayoutGroup() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_LayoutGroup() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_Group() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_Group() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_Continuation() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_AllFlags() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_AllFlags() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);

        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_LayoutContinuation() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_LayoutContinuation() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    public void testCacheGridTabSwitcher_HighEnd_GroupContinuation() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    public void testCacheGridTabSwitcher_LowEnd_GroupContinuation() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    public void testCacheGridAndGroup_LowEnd_enabledThenDisabled_withContinuationFlag() {
        // clang-format on
        when(mCommandLine.hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)).thenReturn(true);
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        CachedFeatureFlags.resetFlagsForTesting();
        // Pretend that we've flipped the continuation flag.
        CachedFeatureFlags.setForTesting(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID, false);
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());
    }
}
