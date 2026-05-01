// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.test.core.app.ApplicationProvider;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;

/** Tests for {@link NtpBackgroundDataCustomizedColor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataCustomizedColorUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testEquals() {
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @ColorInt int lightModeColor = Color.RED;
        @ColorInt int darkModeColor = Color.BLUE;
        @ColorInt int lightModeBackgroundColor = Color.YELLOW;
        @ColorInt int darkModeBackgroundColor = Color.BLACK;

        NtpBackgroundDataCustomizedColor data1 =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        platformType,
                        lightModeColor,
                        darkModeColor,
                        lightModeBackgroundColor,
                        darkModeBackgroundColor);
        NtpBackgroundDataCustomizedColor data2 =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        platformType,
                        lightModeColor,
                        darkModeColor,
                        lightModeBackgroundColor,
                        darkModeBackgroundColor);
        NtpBackgroundDataCustomizedColor data3 =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        platformType,
                        Color.GREEN,
                        darkModeColor,
                        lightModeBackgroundColor,
                        darkModeBackgroundColor);

        assertEquals(data1, data2);
        assertNotEquals(data1, data3);
        assertEquals(data1.hashCode(), data2.hashCode());
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @ColorInt int lightModeColor = Color.RED;
        @ColorInt int darkModeColor = Color.BLUE;
        @ColorInt int lightModeBackgroundColor = Color.YELLOW;
        @ColorInt int darkModeBackgroundColor = Color.BLACK;

        NtpBackgroundDataCustomizedColor data =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        platformType,
                        lightModeColor,
                        darkModeColor,
                        lightModeBackgroundColor,
                        darkModeBackgroundColor);

        JSONObject json = data.toJson();
        NtpBackgroundDataCustomizedColor restored =
                NtpBackgroundDataCustomizedColor.fromJson(mContext, json);

        assertEquals(platformType, restored.getPlatformType());
        assertEquals(NtpBackgroundType.COLOR_FROM_HEX, restored.getBackgroundType());
        assertEquals(lightModeColor, restored.getPrimaryColorLight());
        assertEquals(darkModeColor, restored.getPrimaryColorDark());
        assertEquals(lightModeBackgroundColor, restored.getNtpBackgroundColorLight());
        assertEquals(darkModeBackgroundColor, restored.getNtpBackgroundColorDark());
    }
}
