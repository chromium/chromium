// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.graphics.drawable.LayerDrawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;

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
        List<NtpThemeColorInfo> colorList = NtpThemeColorUtils.createThemeColorList(mContext);
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
}
