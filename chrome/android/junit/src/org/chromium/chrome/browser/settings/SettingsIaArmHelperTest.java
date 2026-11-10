// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link SettingsIaArmHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsIaArmHelperTest {
    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION)
    public void testGetArm_defaultsToDirection1() {
        assertEquals(SettingsIaArmHelper.Arm.DIRECTION_1, SettingsIaArmHelper.getArm());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION + ":arm/direction_2")
    public void testGetArm_direction2() {
        assertEquals(SettingsIaArmHelper.Arm.DIRECTION_2, SettingsIaArmHelper.getArm());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION + ":arm/direction_3")
    public void testGetArm_direction3() {
        assertEquals(SettingsIaArmHelper.Arm.DIRECTION_3, SettingsIaArmHelper.getArm());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IA_REORGANIZATION + ":arm/unknown")
    public void testGetArm_unknownFallsBackToDirection1() {
        assertEquals(SettingsIaArmHelper.Arm.DIRECTION_1, SettingsIaArmHelper.getArm());
    }
}
