// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link NtpBackgroundDataGroup}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataGroupUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        List<NtpBackgroundDataBase> dataList = new ArrayList<>();

        // Add a NtpBackgroundDataColor.
        dataList.add(
                new NtpBackgroundDataColor(
                        mContext,
                        platformType,
                        NtpThemeColorId.NTP_COLORS_ORANGE,
                        /* isChromeColorDailyRefreshEnabled= */ true));

        // Add a NtpBackgroundDataCustomizedColor.
        dataList.add(
                new NtpBackgroundDataCustomizedColor(
                        mContext, platformType, Color.RED, Color.BLUE, Color.YELLOW, Color.BLACK));

        NtpBackgroundDataGroup group = new NtpBackgroundDataGroup(dataList);
        JSONObject json = group.toJson();

        NtpBackgroundDataGroup restoredGroup = NtpBackgroundDataGroup.fromJson(mContext, json);

        assertEquals(2, restoredGroup.size());
        assertTrue(restoredGroup.get(0) instanceof NtpBackgroundDataColor);
        assertEquals(
                NtpThemeColorId.NTP_COLORS_ORANGE,
                ((NtpBackgroundDataColor) restoredGroup.get(0)).getThemeColorId());

        assertTrue(restoredGroup.get(1) instanceof NtpBackgroundDataCustomizedColor);
        assertEquals(
                Color.RED,
                ((NtpBackgroundDataCustomizedColor) restoredGroup.get(1)).getPrimaryColorLight());
    }
}
