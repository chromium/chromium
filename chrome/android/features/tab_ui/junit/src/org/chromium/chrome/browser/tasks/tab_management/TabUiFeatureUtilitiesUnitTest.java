// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build.VERSION_CODES;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Unit Tests for {@link TabUiFeatureUtilities}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiFeatureUtilitiesUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

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
        assertTrue(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                        ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();

        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                        ContextUtils.getApplicationContext()));
    }

    @Test
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd() {
        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                        ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();

        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                        ContextUtils.getApplicationContext()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void testIsTabDragAsWindowEnabled() {
        assertTrue(TabUiFeatureUtilities.isTabDragAsWindowEnabled());
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    public void testIsTabDragDropEnabled() throws NameNotFoundException {
        MultiWindowTestUtils.enableMultiInstance();
        assertTrue(TabUiFeatureUtilities.isTabDragEnabled());
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    @EnableFeatures({
        ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID,
        ChromeFeatureList.TAB_DRAG_DROP_ANDROID
    })
    public void testIsTabDragDropEnabled_bothFlagsEnabled() throws NameNotFoundException {
        MultiWindowTestUtils.enableMultiInstance();
        assertThrows(AssertionError.class, () -> TabUiFeatureUtilities.isTabDragEnabled());
    }

    @Test
    @Config(sdk = VERSION_CODES.Q)
    @EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    public void testIsTabDragDropEnabled_multiInstanceDisabled() {
        assertFalse(TabUiFeatureUtilities.isTabDragEnabled());
    }
}
