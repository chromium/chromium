// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme_history.data.NtpBackgroundDataBase.PlatformType;

/** Tests for {@link NtpBackgroundDataColor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataColorUnitTest {
    @Test
    public void testEquals() {
        @NtpThemeColorId int id1 = NtpThemeColorId.NTP_COLORS_AQUA;
        @NtpThemeColorId int id2 = NtpThemeColorId.NTP_COLORS_BLUE;

        NtpBackgroundDataColor data1 =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID_LOCAL,
                        id1,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor data2 =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID_LOCAL,
                        id1,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor data3 =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID_LOCAL,
                        id2,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor data4 =
                new NtpBackgroundDataColor(
                        PlatformType.IOS, id1, /* isChromeColorDailyRefreshEnabled= */ true);

        assertEquals(data1, data2);
        assertNotEquals(data1, data3);
        assertNotEquals(data1, data4);
        assertEquals(data1.hashCode(), data2.hashCode());
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @NtpThemeColorId int colorId = NtpThemeColorId.NTP_COLORS_AQUA;
        boolean isChromeColorDailyRefreshEnabled = true;

        NtpBackgroundDataColor data =
                new NtpBackgroundDataColor(platformType, colorId, isChromeColorDailyRefreshEnabled);

        JSONObject json = data.toJson();
        NtpBackgroundDataColor restored = NtpBackgroundDataColor.fromJson(json);

        assertEquals(platformType, restored.getPlatformType());
        assertEquals(NtpBackgroundType.CHROME_COLOR, restored.getBackgroundType());
        assertEquals(colorId, restored.getThemeColorId());
        assertEquals(isChromeColorDailyRefreshEnabled, restored.isChromeColorDailyRefreshEnabled());
    }
}
