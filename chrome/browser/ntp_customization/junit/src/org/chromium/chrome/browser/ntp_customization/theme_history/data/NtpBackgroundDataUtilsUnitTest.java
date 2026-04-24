// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Color;
import android.graphics.Matrix;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme_history.data.NtpBackgroundDataBase.PlatformType;

/** Tests for {@link NtpBackgroundDataUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataUtilsUnitTest {
    @Test
    public void testFromJson_Subtypes() throws JSONException {
        // Test that fromJson polymorphic factory works.

        // NtpBackgroundDataColor case.
        JSONObject colorJson = new JSONObject();
        colorJson.put(NtpBackgroundDataBase.PLATFORM_TYPE_KEY, PlatformType.DESKTOP);
        colorJson.put(NtpBackgroundDataBase.BACKGROUND_TYPE_KEY, NtpBackgroundType.CHROME_COLOR);
        colorJson.put(NtpBackgroundDataColor.THEME_COLOR_ID_KEY, 1);
        colorJson.put(NtpBackgroundDataColor.IS_DAILY_REFRESH_ENABLED_KEY, true);

        NtpBackgroundDataBase colorData = NtpBackgroundDataUtils.fromJson(colorJson);
        assertTrue(colorData instanceof NtpBackgroundDataColor);
        assertEquals(1, ((NtpBackgroundDataColor) colorData).getThemeColorId());

        // NtpBackgroundDataCustomizedColor case.
        JSONObject customColorJson = new JSONObject();
        customColorJson.put(NtpBackgroundDataBase.PLATFORM_TYPE_KEY, PlatformType.ANDROID_LOCAL);
        customColorJson.put(
                NtpBackgroundDataBase.BACKGROUND_TYPE_KEY, NtpBackgroundType.COLOR_FROM_HEX);
        customColorJson.put(NtpBackgroundDataCustomizedColor.LIGHT_MODE_COLOR_KEY, Color.GREEN);
        customColorJson.put(NtpBackgroundDataCustomizedColor.DARK_MODE_COLOR_KEY, Color.BLUE);

        NtpBackgroundDataBase customColorData = NtpBackgroundDataUtils.fromJson(customColorJson);
        assertTrue(customColorData instanceof NtpBackgroundDataCustomizedColor);
        assertEquals(
                Color.GREEN,
                ((NtpBackgroundDataCustomizedColor) customColorData).getLightModeColor());
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
}
