// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;

/** Tests for {@link NtpBackgroundDataColor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpBackgroundDataColorUnitTest {
    private Context mContext;
    private NtpBackgroundDataColor mDailyAqua;
    private NtpBackgroundDataColor mDailyBlue;
    private NtpBackgroundDataColor mDailyAquaOtherPlatform;
    private NtpBackgroundDataColor mStaticAqua;
    private NtpBackgroundDataColor mStaticBlue;
    private NtpBackgroundDataColor mStaticAquaOtherPlatform;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mDailyAqua =
                createColor(
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isDailyRefreshEnabled= */ true);
        mDailyBlue =
                createColor(
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isDailyRefreshEnabled= */ true);
        mDailyAquaOtherPlatform =
                createColor(
                        PlatformType.IOS,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isDailyRefreshEnabled= */ true);
        mStaticAqua =
                createColor(
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isDailyRefreshEnabled= */ false);
        mStaticBlue =
                createColor(
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isDailyRefreshEnabled= */ false);
        mStaticAquaOtherPlatform =
                createColor(
                        PlatformType.IOS,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isDailyRefreshEnabled= */ false);
    }

    @Test
    public void testEquals() {
        assertNotNull(mDailyAqua.getImageDrawable());

        NtpBackgroundDataColor dailyAquaCopy =
                createColor(
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isDailyRefreshEnabled= */ true);

        assertEquals(mDailyAqua, dailyAquaCopy);
        assertEquals(mDailyAqua.hashCode(), dailyAquaCopy.hashCode());
        // Two daily colors are the same theme whichever color each device has rotated to.
        assertEquals(mDailyAqua, mDailyBlue);
        assertEquals(mDailyAqua.hashCode(), mDailyBlue.hashCode());
        // The same Chrome color is the same theme wherever it was selected.
        assertEquals(mDailyAqua, mDailyAquaOtherPlatform);
        assertEquals(mDailyAqua.hashCode(), mDailyAquaOtherPlatform.hashCode());
        // Turning daily refresh on or off is a theme change even for the same color.
        assertNotEquals(mDailyAqua, mStaticAqua);
        // Static colors still compare by color ID.
        assertEquals(mStaticAqua, mStaticAquaOtherPlatform);
        assertEquals(mStaticAqua.hashCode(), mStaticAquaOtherPlatform.hashCode());
        assertNotEquals(mStaticAqua, mStaticBlue);
    }

    @Test
    public void testHasSameThemeAndColor() {
        assertTrue(mDailyAqua.hasSameThemeAndColor(mDailyAquaOtherPlatform));
        // A rotated color is a visual change, so hasSameThemeAndColor still compares the color ID.
        assertFalse(mDailyAqua.hasSameThemeAndColor(mDailyBlue));
        assertFalse(mDailyBlue.hasSameThemeAndColor(mDailyAqua));
        // Turning daily refresh on or off is a theme change even for the same color.
        assertFalse(mDailyAqua.hasSameThemeAndColor(mStaticAqua));
        assertFalse(mStaticAqua.hasSameThemeAndColor(mDailyAqua));
        // Static colors still compare by color ID.
        assertTrue(mStaticAqua.hasSameThemeAndColor(mStaticAquaOtherPlatform));
        assertFalse(mStaticAqua.hasSameThemeAndColor(mStaticBlue));
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID;
        @NtpThemeColorId int colorId = NtpThemeColorId.NTP_COLORS_AQUA;
        boolean isChromeColorDailyRefreshEnabled = true;

        NtpBackgroundDataColor data =
                new NtpBackgroundDataColor(
                        mContext, platformType, colorId, isChromeColorDailyRefreshEnabled);

        JSONObject json = data.toJson();
        NtpBackgroundDataColor restored = NtpBackgroundDataColor.fromJson(mContext, json);

        assertEquals(platformType, restored.getPlatformType());
        assertEquals(NtpBackgroundType.CHROME_COLOR, restored.getBackgroundType());
        assertEquals(colorId, restored.getThemeColorId());
        assertEquals(isChromeColorDailyRefreshEnabled, restored.isChromeColorDailyRefreshEnabled());
    }

    @Test
    public void testContentDescription_predefinedColor() {
        testContentDescriptionImpl(
                NtpThemeColorId.NTP_COLORS_AQUA,
                mContext.getString(R.string.accessibility_ntp_aqua_color_theme));
    }

    @Test
    public void testContentDescription_blueColor() {
        testContentDescriptionImpl(
                NtpThemeColorId.NTP_COLORS_BLUE,
                mContext.getString(R.string.accessibility_ntp_blue_color_theme));
    }

    @Test
    public void testContentDescription_defaultColor() {
        testContentDescriptionImpl(NtpThemeColorId.DEFAULT, null);
    }

    @Test
    public void testSetContentDescription() {
        NtpBackgroundDataBase baseData = new NtpBackgroundDataBase(PlatformType.ANDROID);
        assertNull(baseData.getContentDescription());

        String description = "Test Description";
        baseData.setContentDescription(description);
        assertEquals(description, baseData.getContentDescription());
    }

    private void testContentDescriptionImpl(
            @NtpThemeColorId int colorId, @Nullable String expectedDescription) {
        NtpBackgroundDataColor data =
                createColor(PlatformType.ANDROID, colorId, /* isDailyRefreshEnabled= */ false);
        assertEquals(expectedDescription, data.getContentDescription());
    }

    /** Creates a Chrome color with the given platform, color ID and daily refresh setting. */
    private NtpBackgroundDataColor createColor(
            @PlatformType int platformType,
            @NtpThemeColorId int colorId,
            boolean isDailyRefreshEnabled) {
        return new NtpBackgroundDataColor(mContext, platformType, colorId, isDailyRefreshEnabled);
    }
}
