// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;

import androidx.core.content.ContextCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.ui.util.ColorUtils;

/** Tests for {@link NtpThemeColorInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeColorInfoUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testNtpThemeColorInfo() {
        NtpThemeColorInfo info =
                new NtpThemeColorInfo(
                        mContext,
                        NtpThemeColorId.LIGHT_BLUE,
                        R.color.ntp_color_light_blue_background,
                        R.color.ntp_color_light_blue_primary);

        assertEquals(NtpThemeColorId.LIGHT_BLUE, info.id);
        assertEquals(R.color.ntp_color_light_blue_background, info.backgroundColorResId);
        assertEquals(R.color.ntp_color_light_blue_primary, info.primaryColorResId);
        assertNotNull(info.iconDrawable);

        int primaryColor = ContextCompat.getColor(mContext, R.color.ntp_color_light_blue_primary);
        assertEquals(
                ColorUtils.setAlphaComponentWithFloat(primaryColor, 0.15f), info.highlightColor);
    }

    @Test
    public void testNtpThemeColorFromHexInfo() {
        int backgroundColor = ContextCompat.getColor(mContext, R.color.default_red);
        int primaryColor = ContextCompat.getColor(mContext, R.color.default_green);
        NtpThemeColorFromHexInfo info =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);

        assertEquals(backgroundColor, info.backgroundColor);
        assertEquals(primaryColor, info.primaryColor);
        assertNotNull(info.iconDrawable);
        assertEquals(
                ColorUtils.setAlphaComponentWithFloat(primaryColor, 0.15f), info.highlightColor);
    }

    @Test
    public void testEquals() {
        NtpThemeColorInfo lightBlueInfo =
                new NtpThemeColorInfo(
                        mContext,
                        NtpThemeColorId.LIGHT_BLUE,
                        R.color.ntp_color_light_blue_background,
                        R.color.ntp_color_light_blue_primary);

        NtpThemeColorInfo lightBlueInfo2 =
                new NtpThemeColorInfo(
                        mContext,
                        NtpThemeColorId.LIGHT_BLUE,
                        R.color.ntp_color_light_blue_background,
                        R.color.ntp_color_light_blue_primary);

        NtpThemeColorInfo blueInfo =
                new NtpThemeColorInfo(
                        mContext,
                        NtpThemeColorId.BLUE,
                        R.color.ntp_color_blue_background,
                        R.color.ntp_color_blue_primary);

        assertEquals(lightBlueInfo, lightBlueInfo2);
        assertNotEquals(lightBlueInfo, blueInfo);
        assertNotEquals(lightBlueInfo, null);

        int backgroundColor = ContextCompat.getColor(mContext, R.color.default_red);
        int primaryColor = ContextCompat.getColor(mContext, R.color.default_green);
        NtpThemeColorFromHexInfo hexInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        assertNotEquals(lightBlueInfo, hexInfo);
    }
}
