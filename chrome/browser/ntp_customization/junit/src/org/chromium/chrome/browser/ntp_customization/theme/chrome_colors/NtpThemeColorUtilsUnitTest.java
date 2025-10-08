// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link NtpThemeColorUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeColorUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testCreateThemeColorList() {
        List<NtpThemeColorInfo> colorList =
                NtpThemeColorUtils.createThemeColorListForTesting(mContext);
        assertEquals(2, colorList.size());
        assertEquals(NtpThemeColorId.LIGHT_BLUE, colorList.get(0).id);
        assertEquals(NtpThemeColorId.BLUE, colorList.get(1).id);
    }

    @Test
    public void testCreateNtpThemeColorInfo() {
        NtpThemeColorInfo lightBlueInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.LIGHT_BLUE);
        assertNotNull(lightBlueInfo);
        assertEquals(NtpThemeColorId.LIGHT_BLUE, lightBlueInfo.id);

        NtpThemeColorInfo blueInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.BLUE);
        assertNotNull(blueInfo);
        assertEquals(NtpThemeColorId.BLUE, blueInfo.id);

        NtpThemeColorInfo invalidInfo = NtpThemeColorUtils.createNtpThemeColorInfo(mContext, -1);
        assertNull(invalidInfo);
    }

    @Test
    public void testInitColorsListAndFindPrimaryColorIndex_primaryColorNull() {
        verifyInitColorsListAndFindPrimaryColorIndexReturnCorrectIndex(
                /* primaryColorInfo= */ null, RecyclerView.NO_POSITION, /* expectedSize= */ 2);
    }

    @Test
    public void testInitColorsListAndFindPrimaryColorIndex_primaryColorExist() {
        verifyInitColorsListAndFindPrimaryColorIndexReturnCorrectIndex(
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.LIGHT_BLUE),
                /* expectedIndex= */ 1,
                /* expectedSize= */ 2);
    }

    @Test
    public void testInitColorsListAndFindPrimaryColorIndex_customPrimaryColorWithBackgroundColor() {
        @ColorInt int primaryColor = ContextCompat.getColor(mContext, R.color.default_red);
        @ColorInt int backgroundColor = ContextCompat.getColor(mContext, R.color.green_50);
        NtpCustomizationUtils.setBackgroundColorToSharedPreference(backgroundColor);

        verifyInitColorsListAndFindPrimaryColorIndexReturnCorrectIndex(
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor),
                /* expectedIndex= */ 2,
                /* expectedSize= */ 3);
    }

    @Test
    public void testInitColorsListAndFindPrimaryColorIndex_customColorWithoutBackgroundColor() {
        @ColorInt int primaryColor = ContextCompat.getColor(mContext, R.color.default_red);
        verifyInitColorsListAndFindPrimaryColorIndexReturnCorrectIndex(
                new NtpThemeColorFromHexInfo(
                        mContext, NtpThemeColorInfo.COLOR_NOT_SET, primaryColor),
                RecyclerView.NO_POSITION,
                /* expectedSize= */ 2);
    }

    @Test
    public void testInitColorsListAndFindPrimaryColorIndex_colorListNotEmpty() {
        List<NtpThemeColorInfo> colorList =
                NtpThemeColorUtils.createThemeColorListForTesting(mContext);
        colorList.remove(1);
        int originalSize = 1;
        assertEquals(originalSize, colorList.size());
        NtpThemeColorInfo primaryColor =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.LIGHT_BLUE);

        int index =
                NtpThemeColorUtils.initColorsListAndFindPrimaryColorIndex(
                        mContext, colorList, primaryColor);
        assertEquals(originalSize, colorList.size());
        assertEquals(RecyclerView.NO_POSITION, index);
    }

    @Test
    public void testCreateColoredCircle() {
        int topColor = mContext.getColor(R.color.default_red);
        int bottomLeftColor = mContext.getColor(R.color.default_green);
        int bottomRightColor = mContext.getColor(R.color.default_bg_color_blue);

        LayerDrawable drawable =
                NtpThemeColorUtils.createColoredCircle(
                        mContext, topColor, bottomLeftColor, bottomRightColor);
        assertNotNull(drawable);
        assertEquals(3, drawable.getNumberOfLayers());
    }

    @Test
    public void testGetNtpThemePrimaryColorFromPreBuiltColors() {
        @ColorInt
        int lightBluePrimaryColor =
                NtpThemeColorUtils.getNtpThemePrimaryColor(mContext, NtpThemeColorId.LIGHT_BLUE);
        assertEquals(
                mContext.getColor(R.color.ntp_color_light_blue_primary), lightBluePrimaryColor);

        @ColorInt
        int bluePrimaryColor =
                NtpThemeColorUtils.getNtpThemePrimaryColor(mContext, NtpThemeColorId.BLUE);
        assertEquals(mContext.getColor(R.color.ntp_color_blue_primary), bluePrimaryColor);

        assertNull(NtpThemeColorUtils.getNtpThemePrimaryColor(mContext, -1));
    }

    @Test
    public void testIsPrimaryColorMatched_primaryColorFromHex() {
        @ColorInt
        int primaryColor = ContextCompat.getColor(mContext, R.color.ntp_color_light_blue_primary);
        @ColorInt int backgroundColor = ContextCompat.getColor(mContext, R.color.default_red);
        NtpThemeColorFromHexInfo primaryColorInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);

        NtpThemeColorInfo lightBlueInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.LIGHT_BLUE);

        assertEquals(
                false, NtpThemeColorUtils.isPrimaryColorMatched(mContext, primaryColorInfo, null));
        assertEquals(
                true,
                NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, primaryColorInfo, lightBlueInfo));
        assertEquals(
                true,
                NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, primaryColorInfo, primaryColorInfo));

        // Test with another hex info with the same primary color.
        @ColorInt
        int anotherBackgroundColor =
                ContextCompat.getColor(mContext, R.color.default_bg_color_blue);
        NtpThemeColorFromHexInfo samePrimaryHexInfo =
                new NtpThemeColorFromHexInfo(mContext, anotherBackgroundColor, primaryColor);
        assertEquals(
                true,
                NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, primaryColorInfo, samePrimaryHexInfo));

        // Test with another hex info with a different primary color.
        @ColorInt int anotherPrimaryColor = ContextCompat.getColor(mContext, R.color.default_green);
        NtpThemeColorFromHexInfo differentPrimaryHexInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, anotherPrimaryColor);
        assertEquals(
                false,
                NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, primaryColorInfo, differentPrimaryHexInfo));
    }

    @Test
    public void testIsPrimaryColorMatched_primaryColorFromPreBuildColors() {
        NtpThemeColorInfo primaryColorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.LIGHT_BLUE);
        NtpThemeColorInfo lightBlueInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.LIGHT_BLUE);
        NtpThemeColorInfo blueInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.BLUE);

        assertEquals(
                false, NtpThemeColorUtils.isPrimaryColorMatched(mContext, primaryColorInfo, null));
        assertEquals(
                false,
                NtpThemeColorUtils.isPrimaryColorMatched(mContext, primaryColorInfo, blueInfo));
        assertEquals(
                true,
                NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, primaryColorInfo, lightBlueInfo));

        @ColorInt
        int primaryColor = ContextCompat.getColor(mContext, R.color.ntp_color_light_blue_primary);
        @ColorInt int backgroundColor = ContextCompat.getColor(mContext, R.color.green_50);
        NtpThemeColorFromHexInfo anotherHexColorInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        assertEquals(
                true,
                NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, primaryColorInfo, anotherHexColorInfo));
        assertEquals(
                true,
                NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, primaryColorInfo, primaryColorInfo));
    }

    @Test
    public void testGetBackgroundColorFromColorInfo() {
        assertEquals(
                NtpThemeColorUtils.getDefaultBackgroundColor(mContext),
                NtpThemeColorUtils.getBackgroundColorFromColorInfo(mContext, null));

        NtpThemeColorInfo blueInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, NtpThemeColorId.BLUE);
        assertEquals(
                mContext.getColor(R.color.ntp_color_blue_background),
                NtpThemeColorUtils.getBackgroundColorFromColorInfo(mContext, blueInfo));

        @ColorInt int backgroundColor = ContextCompat.getColor(mContext, R.color.green_50);
        NtpThemeColorFromHexInfo customInfo =
                new NtpThemeColorFromHexInfo(
                        mContext, backgroundColor, NtpThemeColorInfo.COLOR_NOT_SET);
        assertEquals(
                backgroundColor,
                NtpThemeColorUtils.getBackgroundColorFromColorInfo(mContext, customInfo));
    }

    @Test
    public void testGetDefaultBackgroundColor() {
        assertEquals(
                ContextCompat.getColor(mContext, R.color.home_surface_background_color),
                NtpThemeColorUtils.getDefaultBackgroundColor(mContext));
    }

    private void verifyInitColorsListAndFindPrimaryColorIndexReturnCorrectIndex(
            @Nullable NtpThemeColorInfo primaryColorInfo, int expectedIndex, int expectedSize) {
        List<NtpThemeColorInfo> colorList = new ArrayList<>();

        int index =
                NtpThemeColorUtils.initColorsListAndFindPrimaryColorIndex(
                        mContext, colorList, primaryColorInfo);
        assertEquals(expectedSize, colorList.size());
        assertEquals(expectedIndex, index);
    }
}
