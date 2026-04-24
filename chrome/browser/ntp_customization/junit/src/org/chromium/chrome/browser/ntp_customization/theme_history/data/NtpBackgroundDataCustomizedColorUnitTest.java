// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.graphics.Color;

import androidx.annotation.ColorInt;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme_history.data.NtpBackgroundDataBase.PlatformType;

/** Tests for {@link NtpBackgroundDataCustomizedColor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataCustomizedColorUnitTest {
    @Test
    public void testEquals() {
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @ColorInt int lightModeColor = Color.RED;
        @ColorInt int darkModeColor = Color.BLUE;

        NtpBackgroundDataCustomizedColor data1 =
                new NtpBackgroundDataCustomizedColor(platformType, lightModeColor, darkModeColor);
        NtpBackgroundDataCustomizedColor data2 =
                new NtpBackgroundDataCustomizedColor(platformType, lightModeColor, darkModeColor);
        NtpBackgroundDataCustomizedColor data3 =
                new NtpBackgroundDataCustomizedColor(platformType, Color.GREEN, darkModeColor);

        assertEquals(data1, data2);
        assertNotEquals(data1, data3);
        assertEquals(data1.hashCode(), data2.hashCode());
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @ColorInt int lightModeColor = Color.RED;
        @ColorInt int darkModeColor = Color.BLUE;

        NtpBackgroundDataCustomizedColor data =
                new NtpBackgroundDataCustomizedColor(platformType, lightModeColor, darkModeColor);

        JSONObject json = data.toJson();
        NtpBackgroundDataCustomizedColor restored = NtpBackgroundDataCustomizedColor.fromJson(json);

        assertEquals(platformType, restored.getPlatformType());
        assertEquals(NtpBackgroundType.COLOR_FROM_HEX, restored.getBackgroundType());
        assertEquals(lightModeColor, restored.getLightModeColor());
        assertEquals(darkModeColor, restored.getDarkModeColor());
    }
}
