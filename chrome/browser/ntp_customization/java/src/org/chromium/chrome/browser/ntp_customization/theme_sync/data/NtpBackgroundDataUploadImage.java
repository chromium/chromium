// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Matrix;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;

import java.util.Objects;

/** Data class for NTP uploaded background image. */
@NullMarked
public class NtpBackgroundDataUploadImage extends NtpBackgroundDataBase {
    @VisibleForTesting
    static final String LAST_UPLOAD_IMAGE_FILE_PATH_KEY = "lastUploadImageFilePath";

    private final String mLastUploadImageFilePath;
    private final @ColorInt int mPrimaryColor;
    private final Matrix mPortraitMatrix;
    private final Matrix mLandscapeMatrix;

    public NtpBackgroundDataUploadImage(
            @PlatformType int platformType,
            String lastUploadImageFilePath,
            @ColorInt int primaryColor,
            Matrix portraitMatrix,
            Matrix landscapeMatrix) {
        super(platformType);
        mLastUploadImageFilePath = lastUploadImageFilePath;
        mPrimaryColor = primaryColor;
        mPortraitMatrix = portraitMatrix;
        mLandscapeMatrix = landscapeMatrix;
    }

    /** Returns the file path of the last uploaded image. */
    public String getLastUploadImageFilePath() {
        return mLastUploadImageFilePath;
    }

    /** Returns the primary color of the background image. */
    public @ColorInt int getPrimaryColor() {
        return mPrimaryColor;
    }

    /** Returns the portrait transformation matrix. */
    public Matrix getPortraitMatrix() {
        return mPortraitMatrix;
    }

    /** Returns the landscape transformation matrix. */
    public Matrix getLandscapeMatrix() {
        return mLandscapeMatrix;
    }

    // NtpBackgroundDataBase implementations.

    @Override
    public @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.IMAGE_FROM_DISK;
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject json = super.toJson();
        json.put(LAST_UPLOAD_IMAGE_FILE_PATH_KEY, mLastUploadImageFilePath);
        json.put(PRIMARY_COLOR_KEY, mPrimaryColor);
        JSONArray portraitMatrixArray = NtpBackgroundDataUtils.matrixToJsonArray(mPortraitMatrix);
        if (portraitMatrixArray != null) {
            json.put(PORTRAIT_MATRIX_KEY, portraitMatrixArray);
        }
        JSONArray landscapeMatrixArray = NtpBackgroundDataUtils.matrixToJsonArray(mLandscapeMatrix);
        if (landscapeMatrixArray != null) {
            json.put(LANDSCAPE_MATRIX_KEY, landscapeMatrixArray);
        }
        return json;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof NtpBackgroundDataUploadImage other) {
            return super.equals(obj)
                    && Objects.equals(mLastUploadImageFilePath, other.mLastUploadImageFilePath)
                    && mPrimaryColor == other.mPrimaryColor;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), mLastUploadImageFilePath, mPrimaryColor);
    }

    /** Returns the NtpBackgroundDataUploadImage object from the given JSON. */
    public static NtpBackgroundDataUploadImage fromJson(JSONObject json) throws JSONException {
        return new NtpBackgroundDataUploadImage(
                json.getInt(PLATFORM_TYPE_KEY),
                json.getString(LAST_UPLOAD_IMAGE_FILE_PATH_KEY),
                json.getInt(PRIMARY_COLOR_KEY),
                assumeNonNull(
                        NtpBackgroundDataUtils.jsonArrayToMatrix(
                                json.getJSONArray(PORTRAIT_MATRIX_KEY))),
                assertNonNull(
                        NtpBackgroundDataUtils.jsonArrayToMatrix(
                                json.getJSONArray(LANDSCAPE_MATRIX_KEY))));
    }
}
