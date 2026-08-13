// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link NtpCustomizationSidePanel}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpCustomizationSidePanelUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NtpCustomizationSidePanel.Natives mNtpCustomizationSidePanelJni;
    @Mock private Tab mMockTab;

    @Before
    public void setUp() {
        NtpCustomizationSidePanelJni.setInstanceForTesting(mNtpCustomizationSidePanelJni);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_CUSTOMIZE_WEBUI_ANDROID})
    public void testIsEnabled_desktopWithFeatureEnabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(NtpCustomizationSidePanel.isEnabled());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_CUSTOMIZE_WEBUI_ANDROID})
    public void testIsEnabled_nonDesktopWithFeatureEnabled() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(NtpCustomizationSidePanel.isEnabled());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.NTP_CUSTOMIZE_WEBUI_ANDROID})
    public void testIsEnabled_desktopWithFeatureDisabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertFalse(NtpCustomizationSidePanel.isEnabled());
    }

    @Test
    public void testShow_withTab() {
        NtpCustomizationSidePanel.show(mMockTab);
        verify(mNtpCustomizationSidePanelJni).show(eq(mMockTab));
    }
}
