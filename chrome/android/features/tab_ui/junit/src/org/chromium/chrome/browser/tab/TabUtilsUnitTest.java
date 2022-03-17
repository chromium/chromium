// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tab;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.res.Configuration;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for {@link TabUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabUtilsUnitTest {
    @Test
    public void testGetTabThumbnailAspectRatio_withNullContext() {
        assertThat(TabUtils.getTabThumbnailAspectRatio(null))
                .isEqualTo(TabUtils.TAB_THUMBNAIL_ASPECT_RATIO);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testGetTabThumbnailAspectRatio_withNonTabletContext() {
        assertThat(TabUtils.getTabThumbnailAspectRatio(ContextUtils.getApplicationContext()))
                .isEqualTo(TabUtils.TAB_THUMBNAIL_ASPECT_RATIO);
    }

    @Test
    @Config(qualifiers = "sw800dp-port")
    public void testGetTabThumbnailAspectRatio_withTabletPortraitContext() {
        assertThat(TabUtils.getTabThumbnailAspectRatio(ContextUtils.getApplicationContext()))
                .isEqualTo(TabUtils.TAB_THUMBNAIL_ASPECT_RATIO);
    }

    @Test
    @Config(qualifiers = "sw800dp-land")
    public void testGetTabThumbnailAspectRatio_withTabletLandscapeContext() {
        final Context applicationContext = ContextUtils.getApplicationContext();
        final Configuration configuration = applicationContext.getResources().getConfiguration();
        float expectedAspectRatio =
                (configuration.screenWidthDp * 1f) / (configuration.screenHeightDp * 1f);
        assertThat(TabUtils.getTabThumbnailAspectRatio(applicationContext))
                .isEqualTo(expectedAspectRatio);
    }
}