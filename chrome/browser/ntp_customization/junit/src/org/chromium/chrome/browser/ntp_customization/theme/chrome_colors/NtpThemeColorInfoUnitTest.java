// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.ColorInt;
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
        NtpThemeColorInfo info = new NtpThemeColorInfo(mContext, NtpThemeColorId.NTP_COLORS_AQUA);

        assertEquals(NtpThemeColorId.NTP_COLORS_AQUA, info.id);
        assertEquals(R.color.ntp_color_aqua_primary, info.primaryColorResId);
        assertEquals(R.string.accessibility_ntp_aqua_color_theme, info.colorStringResId);
        assertNotNull(info.iconDrawable);

        int primaryColor = ContextCompat.getColor(mContext, R.color.ntp_color_aqua_primary);
        assertEquals(
                ColorUtils.setAlphaComponentWithFloat(primaryColor, 0.3f), info.highlightColor);
    }

    @Test
    public void testNtpThemeColorInfo_default() {
        NtpThemeColorInfo info = new NtpThemeColorInfo(mContext, NtpThemeColorId.DEFAULT);

        assertEquals(NtpThemeColorId.DEFAULT, info.id);
        assertNotNull(info.iconDrawable);
    }

    @Test
    public void testNtpThemeColorFromHexInfo() {
        @ColorInt int backgroundColor = ContextCompat.getColor(mContext, R.color.default_red);
        @ColorInt int primaryColor = ContextCompat.getColor(mContext, R.color.default_green);
        NtpThemeColorFromHexInfo info =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);

        assertEquals(backgroundColor, info.backgroundColorLight);
        assertEquals(primaryColor, info.primaryColorLight);
        assertNotNull(info.iconDrawable);
        assertEquals(
                ColorUtils.setAlphaComponentWithFloat(primaryColor, 0.3f), info.highlightColor);
    }

    @Test
    public void testNtpThemeColorFromHexInfo_withLightDarkColors() {
        @ColorInt int backgroundColorLight = Color.RED;
        @ColorInt int backgroundColorDark = Color.BLUE;
        @ColorInt int primaryColorLight = Color.GREEN;
        @ColorInt int primaryColorDark = Color.YELLOW;
        NtpThemeColorFromHexInfo info =
                new NtpThemeColorFromHexInfo(
                        mContext,
                        backgroundColorLight,
                        backgroundColorDark,
                        primaryColorLight,
                        primaryColorDark);

        assertEquals(backgroundColorLight, info.backgroundColorLight);
        assertEquals(backgroundColorDark, info.backgroundColorDark);
        assertEquals(primaryColorLight, info.primaryColorLight);
        assertEquals(primaryColorDark, info.primaryColorDark);
        assertNotNull(info.iconDrawable);
        assertEquals(
                ColorUtils.setAlphaComponentWithFloat(primaryColorLight, 0.3f),
                info.highlightColor);
    }

    @Test
    public void testEquals() {
        NtpThemeColorInfo aquaInfo =
                new NtpThemeColorInfo(mContext, NtpThemeColorId.NTP_COLORS_AQUA);

        NtpThemeColorInfo aquaInfo2 =
                new NtpThemeColorInfo(mContext, NtpThemeColorId.NTP_COLORS_AQUA);

        NtpThemeColorInfo blueInfo =
                new NtpThemeColorInfo(mContext, NtpThemeColorId.NTP_COLORS_BLUE);

        assertEquals(aquaInfo, aquaInfo2);
        assertNotEquals(aquaInfo, blueInfo);
        assertNotEquals(aquaInfo, null);

        int backgroundColor = ContextCompat.getColor(mContext, R.color.default_red);
        int primaryColor = ContextCompat.getColor(mContext, R.color.default_green);
        NtpThemeColorFromHexInfo hexInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        NtpThemeColorFromHexInfo hexInfo1 =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        assertNotEquals(aquaInfo, hexInfo);
        assertEquals(hexInfo, hexInfo1);
    }

    @Test
    public void testCalculateHighlightColor() {
        @ColorInt int primaryColor = ContextCompat.getColor(mContext, R.color.default_green);
        @ColorInt
        int expectedHighlightColor = ColorUtils.setAlphaComponentWithFloat(primaryColor, 0.3f);

        assertEquals(
                expectedHighlightColor,
                NtpThemeColorInfo.calculateHighlightColorForColorPalette(primaryColor));
    }

    @Test
    public void testCalculateBackgroundColor() {
        @ColorInt int primaryColor = ContextCompat.getColor(mContext, R.color.default_green);
        @ColorInt
        int expectedBackgroundColor = ColorUtils.setAlphaComponentWithFloat(primaryColor, 0.15f);

        assertEquals(
                expectedBackgroundColor,
                NtpThemeColorInfo.calculateBackgroundColorForColorPalette(primaryColor));
    }
}
