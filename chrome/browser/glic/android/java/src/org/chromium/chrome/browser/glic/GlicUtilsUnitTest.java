// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link GlicUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testIsSidePanelFormFactor_Desktop() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(GlicUtils.isSidePanelFormFactor(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_ANDROID_TABLET)
    public void testIsSidePanelFormFactor_Phone_DisabledEvenWithTabletFlag() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(GlicUtils.isSidePanelFormFactor(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures(ChromeFeatureList.GLIC_ANDROID_TABLET)
    public void testIsSidePanelFormFactor_Tablet_DisabledWhenTabletFlagDisabled() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(GlicUtils.isSidePanelFormFactor(mContext));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.GLIC_ANDROID_TABLET)
    public void testIsSidePanelFormFactor_Tablet_EnabledWhenTabletFlagEnabled() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertTrue(GlicUtils.isSidePanelFormFactor(mContext));
    }

    @Test
    public void testIsSidePanelFormFactor_TestingOverride() {
        GlicUtils.setIsSidePanelFormFactorForTesting(true);
        assertTrue(GlicUtils.isSidePanelFormFactor(mContext));

        GlicUtils.setIsSidePanelFormFactorForTesting(false);
        assertFalse(GlicUtils.isSidePanelFormFactor(mContext));
    }
}
