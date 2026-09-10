// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

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
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testIsEnabled_FeatureDisabled_ReturnsFalse() {
        assertFalse(SettingsInTab.isEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    @Config(qualifiers = "sw600dp")
    public void testIsEnabled_SettingsMultiColumnDisabled_ReturnsFalse() {
        assertFalse(SettingsInTab.isEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testIsEnabled_FeatureDisabledOnTablet_ReturnsFalse() {
        assertFalse(SettingsInTab.isEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw320dp")
    public void testIsEnabled_FeatureEnabledOnPhone_ReturnsFalse() {
        assertFalse(SettingsInTab.isEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testIsEnabled_FeatureEnabledOnTablet_ReturnsTrue() {
        assertTrue(SettingsInTab.isEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP)
    public void testIsEnabled_Desktop_SettingsInTabDisabled_ReturnsTrue() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(SettingsInTab.isEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP)
    public void testIsEnabled_Desktop_SettingsInTabDesktopDisabled_ReturnsFalse() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertFalse(SettingsInTab.isEnabled());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testIsEnabled_FeatureEnabledOnDesktopNarrowWindow_ReturnsTrue() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(SettingsInTab.isEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testIsEnabled_WithResumedActivity_ReturnsTrue() {
        Activity activity =
                Robolectric.buildActivity(Activity.class).create().start().resume().get();
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        assertTrue(SettingsInTab.isEnabled());
    }
}
