// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;

import java.util.Objects;

/** Data class for NTP uploaded background image. */
@NullMarked
public class NtpBackgroundDataUploadImage extends NtpBackgroundDataBase {
    @VisibleForTesting
    static final String LAST_UPLOAD_IMAGE_FILE_PATH_KEY = "lastUploadImageFilePath";

    private final String mLastUploadImageFilePath;
    private final @Nullable BackgroundImageInfo mBackgroundImageInfo;
    private final @Nullable Bitmap mBitmap;
    private @Nullable @ColorInt Integer mPrimaryColor;

    /**
     * @param platformType The platform type of the device.
     * @param lastUploadImageFilePath The file path of the last uploaded image.
     * @param backgroundImageInfo The background image info containing matrices and window sizes.
     * @param bitmap The local bitmap, not synced.
     * @param primaryColor The primary color of the background image.
     */
    public NtpBackgroundDataUploadImage(
            @PlatformType int platformType,
            String lastUploadImageFilePath,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @Nullable Bitmap bitmap,
            @Nullable @ColorInt Integer primaryColor) {
        super(platformType);
        mLastUploadImageFilePath = lastUploadImageFilePath;
        mBackgroundImageInfo = backgroundImageInfo;
        mBitmap = bitmap;
        mPrimaryColor = primaryColor;
    }

    /** Returns the file path of the last uploaded image. */
    public String getLastUploadImageFilePath() {
        return mLastUploadImageFilePath;
    }

    /** Returns the background image info containing matrices and window sizes. */
    public @Nullable BackgroundImageInfo getBackgroundImageInfo() {
        return mBackgroundImageInfo;
    }

    /** Returns the local bitmap, which is not synced. */
    public @Nullable Bitmap getBitmap() {
        return mBitmap;
    }

    /**
     * Sets the primary color of the background image.
     *
     * @param primaryColor The primary color to set.
     */
    public void setPrimaryColor(@ColorInt int primaryColor) {
        mPrimaryColor = primaryColor;
    }

    /** Returns the primary color of the background image. */
    public @Nullable @ColorInt Integer getPrimaryColor() {
        return mPrimaryColor;
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
        if (mBackgroundImageInfo != null) {
            json.put(BACKGROUND_IMAGE_INFO_KEY, mBackgroundImageInfo.toJson());
        }
        return json;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof NtpBackgroundDataUploadImage other) {
            return super.equals(obj)
                    && Objects.equals(mLastUploadImageFilePath, other.mLastUploadImageFilePath)
                    && Objects.equals(mPrimaryColor, other.mPrimaryColor);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), mLastUploadImageFilePath, mPrimaryColor);
    }

    /** Returns the NtpBackgroundDataUploadImage object from the given JSON. */
    public static NtpBackgroundDataUploadImage fromJson(JSONObject json) throws JSONException {
        BackgroundImageInfo backgroundImageInfo = null;
        if (json.has(BACKGROUND_IMAGE_INFO_KEY)) {
            backgroundImageInfo =
                    BackgroundImageInfo.fromJson(json.getJSONObject(BACKGROUND_IMAGE_INFO_KEY));
        }
        return new NtpBackgroundDataUploadImage(
                json.getInt(PLATFORM_TYPE_KEY),
                json.getString(LAST_UPLOAD_IMAGE_FILE_PATH_KEY),
                backgroundImageInfo,
                /* bitmap= */ null,
                json.getInt(PRIMARY_COLOR_KEY));
    }
}
