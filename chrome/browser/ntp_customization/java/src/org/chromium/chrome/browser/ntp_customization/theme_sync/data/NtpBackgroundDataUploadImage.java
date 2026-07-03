// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;

import java.util.Objects;

/** Data class for NTP uploaded background image. */
@NullMarked
public class NtpBackgroundDataUploadImage extends NtpBackgroundDataBase {
    @VisibleForTesting
    static final String LAST_UPLOAD_IMAGE_FILE_PATH_KEY = "lastUploadImageFilePath";

    private final @Nullable BackgroundImageInfo mBackgroundImageInfo;
    // The mFileIdHash isn't null when NTP theme sync is enabled.
    private final @Nullable String mFileIdHash;
    // The mLastUploadImageFilePath isn't null when mFileIdHash isn't null.
    private final @Nullable String mLastUploadImageFilePath;
    private @Nullable Bitmap mBitmap;
    private @Nullable @ColorInt Integer mPrimaryColor;

    /**
     * @param platformType The platform type of the device.
     * @param backgroundImageInfo The background image info containing matrices and window sizes.
     * @param bitmap The local bitmap, not synced.
     * @param primaryColor The primary color of the background image.
     * @param fileIdHash The ID hash of the image file.
     */
    public NtpBackgroundDataUploadImage(
            @PlatformType int platformType,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @Nullable Bitmap bitmap,
            @Nullable @ColorInt Integer primaryColor,
            @Nullable String fileIdHash) {
        super(platformType);
        mBackgroundImageInfo = backgroundImageInfo;
        mBitmap = bitmap;
        mPrimaryColor = primaryColor;
        mFileIdHash = fileIdHash;

        if (mFileIdHash != null) {
            mLastUploadImageFilePath =
                    NtpCustomizationUtils.createUploadImageFileInDir(mFileIdHash).getAbsolutePath();
        } else {
            mLastUploadImageFilePath = null;
        }
    }

    /** Returns the file path of the last uploaded image. */
    public @Nullable String getLastUploadImageFilePath() {
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
    public void setPrimaryColor(@Nullable @ColorInt Integer primaryColor) {
        mPrimaryColor = primaryColor;
    }

    /** Returns the primary color of the background image. */
    public @Nullable @ColorInt Integer getPrimaryColor() {
        return mPrimaryColor;
    }

    /** Returns the file ID hash of the background image. */
    public @Nullable String getFileIdHash() {
        return mFileIdHash;
    }

    // NtpBackgroundDataBase implementations.

    @Override
    public @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.IMAGE_FROM_DISK;
    }

    @Override
    public void getBitmapOrLoadImage(Callback<@Nullable Bitmap> onImageLoadedCallback) {
        if (mBitmap != null) {
            onImageLoadedCallback.onResult(mBitmap);
            return;
        }

        NtpBackgroundDataBase currentBackgroundData =
                NtpCustomizationConfigManager.getInstance().getNtpBackgroundData();
        if (currentBackgroundData instanceof NtpBackgroundDataUploadImage uploadImageData
                && Objects.equals(currentBackgroundData, this)) {
            mBitmap = uploadImageData.getBitmap();
            onImageLoadedCallback.onResult(mBitmap);
        } else {
            NtpBackgroundDataUtils.loadImage(
                    (result) -> {
                        mBitmap = result;
                        onImageLoadedCallback.onResult(mBitmap);
                    },
                    mLastUploadImageFilePath);
        }
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject json = super.toJson();
        json.put(PRIMARY_COLOR_KEY, mPrimaryColor);
        if (mLastUploadImageFilePath != null) {
            json.put(LAST_UPLOAD_IMAGE_FILE_PATH_KEY, mLastUploadImageFilePath);
        }
        if (mFileIdHash != null) {
            json.put(FILE_ID_HASH_KEY, mFileIdHash);
        }
        if (mBackgroundImageInfo != null) {
            json.put(BACKGROUND_IMAGE_INFO_KEY, mBackgroundImageInfo.toJson());
        }
        return json;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof NtpBackgroundDataUploadImage other) {
            return super.equals(obj)
                    && Objects.equals(mPrimaryColor, other.mPrimaryColor)
                    && Objects.equals(mFileIdHash, other.mFileIdHash)
                    && Objects.equals(mBackgroundImageInfo, other.mBackgroundImageInfo);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                super.hashCode(),
                mLastUploadImageFilePath,
                mPrimaryColor,
                mFileIdHash,
                mBackgroundImageInfo);
    }

    @Override
    public @Nullable Bitmap getImageBitmapForTesting() {
        return mBitmap;
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
                backgroundImageInfo,
                /* bitmap= */ null,
                json.has(PRIMARY_COLOR_KEY) ? json.getInt(PRIMARY_COLOR_KEY) : null,
                json.has(FILE_ID_HASH_KEY) ? json.getString(FILE_ID_HASH_KEY) : null);
    }
}
