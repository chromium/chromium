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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
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
                /* primaryColor= */ null, RecyclerView.NO_POSITION);
    }

    @Test
    public void testInitColorsListAndFindPrimaryColorIndex_primaryColorExist() {
        verifyInitColorsListAndFindPrimaryColorIndexReturnCorrectIndex(
                ContextCompat.getColor(mContext, R.color.ntp_color_light_blue_primary), 1);
    }

    @Test
    public void testInitColorsListAndFindPrimaryColorIndex_primaryColorDoesNotExist() {
        verifyInitColorsListAndFindPrimaryColorIndexReturnCorrectIndex(
                ContextCompat.getColor(mContext, R.color.default_red), RecyclerView.NO_POSITION);
    }

    @Test
    public void testInitColorsListAndFindPrimaryColorIndex_colorListNotEmpty() {
        List<NtpThemeColorInfo> colorList =
                NtpThemeColorUtils.createThemeColorListForTesting(mContext);
        colorList.remove(1);
        int originalSize = 1;
        assertEquals(originalSize, colorList.size());
        @ColorInt
        Integer primaryColor =
                ContextCompat.getColor(mContext, R.color.ntp_color_light_blue_primary);

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

    private void verifyInitColorsListAndFindPrimaryColorIndexReturnCorrectIndex(
            @Nullable @ColorInt Integer primaryColor, int expectedIndex) {
        List<NtpThemeColorInfo> colorList = new ArrayList<>();

        int index =
                NtpThemeColorUtils.initColorsListAndFindPrimaryColorIndex(
                        mContext, colorList, primaryColor);
        assertEquals(2, colorList.size());
        assertEquals(expectedIndex, index);
    }
}
