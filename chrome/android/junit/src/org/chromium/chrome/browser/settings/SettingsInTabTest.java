// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.os.Build;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link SettingsInTab}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsInTabTest {
    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
        DeviceInfo.resetIsFoldableForTesting();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testShouldOpenSettingsInTab_FeatureDisabled_ReturnsFalse() {
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    @Config(qualifiers = "sw600dp")
    public void testShouldOpenSettingsInTab_SettingsMultiColumnDisabled_ReturnsFalse() {
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testShouldOpenSettingsInTab_FeatureDisabledOnTablet_ReturnsFalse() {
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw320dp")
    public void testShouldOpenSettingsInTab_FeatureEnabledOnPhone_ReturnsFalse() {
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testShouldOpenSettingsInTab_FeatureEnabledOnTablet_ReturnsTrue() {
        assertTrue(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testShouldOpenSettingsInTab_FeatureEnabledOnAutomotive_ReturnsFalse() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "w1600dp-h1200dp")
    public void testShouldOpenSettingsInTab_FeatureEnabledOnLargeAutomotive_ReturnsTrue() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertTrue(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP)
    public void testShouldOpenSettingsInTab_Desktop_SettingsInTabDisabled_ReturnsTrue() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP)
    public void testShouldOpenSettingsInTab_Desktop_SettingsInTabDesktopDisabled_ReturnsFalse() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShouldOpenSettingsInTab_FeatureEnabledOnDesktopNarrowWindow_ReturnsTrue() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testShouldOpenSettingsInTab_WithResumedActivity_ReturnsTrue() {
        Activity activity =
                Robolectric.buildActivity(Activity.class).create().start().resume().get();
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        assertTrue(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw320dp")
    public void testShouldOpenSettingsInTab_FoldedFoldable_ReturnsFalse() {
        DeviceInfo.setIsFoldableForTesting(true);
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testShouldOpenSettingsInTab_UnfoldedFoldable_ReturnsTrue() {
        DeviceInfo.setIsFoldableForTesting(true);
        assertTrue(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw320dp")
    public void testShouldOpenSettingsInTab_NonFoldablePhone_ReturnsFalse() {
        DeviceInfo.setIsFoldableForTesting(false);
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testIsFeatureEnabled_FeatureDisabled_ReturnsFalse() {
        assertFalse(SettingsInTab.isFeatureEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testIsFeatureEnabled_SettingsMultiColumnDisabled_ReturnsFalse() {
        assertFalse(SettingsInTab.isFeatureEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw320dp")
    public void testIsFeatureEnabled_IgnoresScreenWidth() {
        // A narrow screen must not disable the feature, otherwise an already-open settings tab
        // could not be recreated after the screen width changes. See crbug.com/562619494.
        assertTrue(SettingsInTab.isFeatureEnabled());
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testIsFeatureEnabled_IgnoresFoldable() {
        DeviceInfo.setIsFoldableForTesting(true);
        assertTrue(SettingsInTab.isFeatureEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP)
    public void testIsFeatureEnabled_Desktop_UsesDesktopFlag() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(SettingsInTab.isFeatureEnabled());
    }
}
