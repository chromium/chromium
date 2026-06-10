// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.json.JSONArray;
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

/** Tests for {@link NtpBackgroundDataUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testFromJson_Subtypes() throws JSONException {
        // Test that fromJson polymorphic factory works.

        // NtpBackgroundDataColor case.
        JSONObject colorJson = new JSONObject();
        colorJson.put(NtpBackgroundDataBase.PLATFORM_TYPE_KEY, PlatformType.DESKTOP);
        colorJson.put(NtpBackgroundDataBase.BACKGROUND_TYPE_KEY, NtpBackgroundType.CHROME_COLOR);
        colorJson.put(NtpBackgroundDataColor.THEME_COLOR_ID_KEY, 1);
        colorJson.put(NtpBackgroundDataColor.IS_DAILY_REFRESH_ENABLED_KEY, true);

        NtpBackgroundDataBase colorData = NtpBackgroundDataUtils.fromJson(mContext, colorJson);
        assertTrue(colorData instanceof NtpBackgroundDataColor);
        assertEquals(1, ((NtpBackgroundDataColor) colorData).getThemeColorId());

        // NtpBackgroundDataCustomizedColor case.
        JSONObject customColorJson = new JSONObject();
        customColorJson.put(NtpBackgroundDataBase.PLATFORM_TYPE_KEY, PlatformType.ANDROID_LOCAL);
        customColorJson.put(
                NtpBackgroundDataBase.BACKGROUND_TYPE_KEY, NtpBackgroundType.COLOR_FROM_HEX);
        customColorJson.put(
                NtpBackgroundDataCustomizedColor.PRIMARY_COLOR_LIGHT_MODE_KEY, Color.GREEN);
        customColorJson.put(
                NtpBackgroundDataCustomizedColor.PRIMARY_COLOR_DARK_MODE_KEY, Color.BLUE);
        customColorJson.put(
                NtpBackgroundDataCustomizedColor.NTP_BACKGROUND_COLOR_LIGHT_MODE_KEY, Color.YELLOW);
        customColorJson.put(
                NtpBackgroundDataCustomizedColor.NTP_BACKGROUND_COLOR_DARK_MODE_KEY, Color.BLACK);

        NtpBackgroundDataBase customColorData =
                NtpBackgroundDataUtils.fromJson(mContext, customColorJson);
        assertTrue(customColorData instanceof NtpBackgroundDataCustomizedColor);
        assertEquals(
                Color.GREEN,
                ((NtpBackgroundDataCustomizedColor) customColorData).getPrimaryColorLight());
        assertEquals(
                Color.BLUE,
                ((NtpBackgroundDataCustomizedColor) customColorData).getPrimaryColorDark());
        assertEquals(
                Color.YELLOW,
                ((NtpBackgroundDataCustomizedColor) customColorData).getNtpBackgroundColorLight());
        assertEquals(
                Color.BLACK,
                ((NtpBackgroundDataCustomizedColor) customColorData).getNtpBackgroundColorDark());
    }

    @Test
    public void testMatrixConversion() throws JSONException {
        Matrix matrix = new Matrix();
        float[] values = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        matrix.setValues(values);

        JSONArray jsonArray = NtpBackgroundDataUtils.matrixToJsonArray(matrix);
        assertNotNull(jsonArray);
        assertEquals(9, jsonArray.length());
        for (int i = 0; i < 9; i++) {
            assertEquals(values[i], (float) jsonArray.getDouble(i), 0.001f);
        }

        Matrix restoredMatrix = NtpBackgroundDataUtils.jsonArrayToMatrix(jsonArray);
        assertNotNull(restoredMatrix);
        float[] restoredValues = new float[9];
        restoredMatrix.getValues(restoredValues);
        for (int i = 0; i < 9; i++) {
            assertEquals(values[i], restoredValues[i], 0.001f);
        }

        assertNull(NtpBackgroundDataUtils.matrixToJsonArray(null));
        assertNull(NtpBackgroundDataUtils.jsonArrayToMatrix(null));
    }

    @Test
    public void testPointConversion() throws JSONException {
        Point point = new Point(123, 456);

        JSONArray jsonArray = NtpBackgroundDataUtils.pointToJsonArray(point);
        assertNotNull(jsonArray);
        assertEquals(2, jsonArray.length());
        assertEquals(123, jsonArray.getInt(0));
        assertEquals(456, jsonArray.getInt(1));

        Point restoredPoint = NtpBackgroundDataUtils.jsonArrayToPoint(jsonArray);
        assertNotNull(restoredPoint);
        assertEquals(point, restoredPoint);

        assertNull(NtpBackgroundDataUtils.pointToJsonArray(null));
        assertNull(NtpBackgroundDataUtils.jsonArrayToPoint(null));
    }

    @Test
    public void testJsonArrayToPoint_invalidLength_returnsNull() throws JSONException {
        JSONArray jsonArray = new JSONArray();
        jsonArray.put(1);
        assertNull(NtpBackgroundDataUtils.jsonArrayToPoint(jsonArray));
    }
}
