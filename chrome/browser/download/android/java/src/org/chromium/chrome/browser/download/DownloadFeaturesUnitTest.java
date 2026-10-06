// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link DownloadFeatures}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadFeaturesUnitTest {
    @Test
    @EnableFeatures(ChromeFeatureList.DOWNLOAD_TOOLBAR_BUTTON_FOR_DESKTOP)
    public void testIsDownloadToolbarButtonEnabled_featureEnabled_desktop() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        assertTrue(DownloadFeatures.isDownloadToolbarButtonEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DOWNLOAD_TOOLBAR_BUTTON_FOR_DESKTOP)
    public void testIsDownloadToolbarButtonEnabled_featureEnabled_notDesktop() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        assertFalse(DownloadFeatures.isDownloadToolbarButtonEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DOWNLOAD_TOOLBAR_BUTTON_FOR_DESKTOP)
    public void testIsDownloadToolbarButtonEnabled_featureDisabled_desktop() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        assertFalse(DownloadFeatures.isDownloadToolbarButtonEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DOWNLOAD_TOOLBAR_BUTTON_FOR_DESKTOP)
    public void testIsDownloadToolbarButtonEnabled_featureDisabled_notDesktop() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ false);
        assertFalse(DownloadFeatures.isDownloadToolbarButtonEnabled());
    }
}
