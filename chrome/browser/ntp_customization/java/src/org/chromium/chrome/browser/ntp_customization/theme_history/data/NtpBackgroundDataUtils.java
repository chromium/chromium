// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import static org.chromium.chrome.browser.ntp_customization.theme_history.data.NtpBackgroundDataBase.BACKGROUND_TYPE_KEY;

import android.graphics.Matrix;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;

/** Utility class for NTP background data conversion. */
@NullMarked
public class NtpBackgroundDataUtils {
    /**
     * Returns the NtpBackgroundDataBase object from the given JSON representation. Null if the type
     * is invalid.
     */
    public static @Nullable NtpBackgroundDataBase fromJson(JSONObject jsonObject)
            throws JSONException {
        int backgroundType = jsonObject.getInt(BACKGROUND_TYPE_KEY);
        switch (backgroundType) {
            case NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR:
                return NtpBackgroundDataColor.fromJson(jsonObject);
            case NtpCustomizationUtils.NtpBackgroundType.COLOR_FROM_HEX:
                return NtpBackgroundDataCustomizedColor.fromJson(jsonObject);
            case NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK:
                return NtpBackgroundDataUploadImage.fromJson(jsonObject);
            case NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION:
                return NtpBackgroundDataThemeCollection.fromJson(jsonObject);
            default:
                return null;
        }
    }

    /**
     * Converts a {@link JSONArray} to a {@link Matrix}.
     *
     * @param jsonArray The {@link JSONArray} to convert.
     * @return The {@link Matrix} represented by the {@link JSONArray}.
     * @throws JSONException If the {@link JSONArray} is not a valid representation of a {@link
     *     Matrix}.
     */
    static @Nullable Matrix jsonArrayToMatrix(@Nullable JSONArray jsonArray) throws JSONException {
        if (jsonArray == null) return null;

        float[] values = new float[9];
        for (int i = 0; i < 9; i++) {
            values[i] = (float) jsonArray.getDouble(i);
        }
        Matrix matrix = new Matrix();
        matrix.setValues(values);
        return matrix;
    }

    /**
     * Converts a {@link Matrix} to a {@link JSONArray}.
     *
     * @param matrix The {@link Matrix} to convert.
     * @return The {@link JSONArray} representation of the {@link Matrix}.
     * @throws JSONException If there is an error creating the {@link JSONArray}.
     */
    static @Nullable JSONArray matrixToJsonArray(@Nullable Matrix matrix) throws JSONException {
        if (matrix == null) return null;

        float[] values = new float[9];
        matrix.getValues(values);
        JSONArray jsonArray = new JSONArray();
        for (float v : values) {
            jsonArray.put(v);
        }
        return jsonArray;
    }
}
