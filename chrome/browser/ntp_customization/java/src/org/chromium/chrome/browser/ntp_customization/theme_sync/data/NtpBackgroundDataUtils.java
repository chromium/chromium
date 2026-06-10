// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.BACKGROUND_TYPE_KEY;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Point;

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
    public static @Nullable NtpBackgroundDataBase fromJson(Context context, JSONObject jsonObject)
            throws JSONException {
        int backgroundType = jsonObject.getInt(BACKGROUND_TYPE_KEY);
        switch (backgroundType) {
            case NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR:
                return NtpBackgroundDataColor.fromJson(context, jsonObject);
            case NtpCustomizationUtils.NtpBackgroundType.COLOR_FROM_HEX:
                return NtpBackgroundDataCustomizedColor.fromJson(context, jsonObject);
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
     */
    public static @Nullable Matrix jsonArrayToMatrix(@Nullable JSONArray jsonArray) {
        if (jsonArray == null) return null;

        try {
            float[] values = new float[9];
            for (int i = 0; i < 9; i++) {
                values[i] = (float) jsonArray.getDouble(i);
            }
            Matrix matrix = new Matrix();
            matrix.setValues(values);
            return matrix;
        } catch (JSONException e) {
            return null;
        }
    }

    /**
     * Converts a {@link Matrix} to a {@link JSONArray}.
     *
     * @param matrix The {@link Matrix} to convert.
     * @return The {@link JSONArray} representation of the {@link Matrix}.
     */
    public static @Nullable JSONArray matrixToJsonArray(@Nullable Matrix matrix) {
        if (matrix == null) return null;

        try {
            float[] values = new float[9];
            matrix.getValues(values);
            JSONArray jsonArray = new JSONArray();
            for (float v : values) {
                jsonArray.put(v);
            }
            return jsonArray;
        } catch (JSONException e) {
            return null;
        }
    }

    /** Converts a {@link Point} to a {@link JSONArray}. */
    public static @Nullable JSONArray pointToJsonArray(@Nullable Point point) {
        if (point == null) return null;

        JSONArray jsonArray = new JSONArray();
        jsonArray.put(point.x);
        jsonArray.put(point.y);
        return jsonArray;
    }

    /** Converts a {@link JSONArray} to a {@link Point}. */
    public static @Nullable Point jsonArrayToPoint(@Nullable JSONArray jsonArray) {
        if (jsonArray == null) return null;

        try {
            if (jsonArray.length() != 2) {
                return null;
            }
            return new Point(jsonArray.getInt(0), jsonArray.getInt(1));
        } catch (JSONException e) {
            return null;
        }
    }
}
