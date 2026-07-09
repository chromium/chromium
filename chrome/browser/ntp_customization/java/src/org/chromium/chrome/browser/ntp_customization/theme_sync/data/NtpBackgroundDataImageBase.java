// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.graphics.Bitmap;

import androidx.annotation.ColorInt;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;

import java.util.Objects;

/** Base data class for NTP image-based background data. */
@NullMarked
public abstract class NtpBackgroundDataImageBase extends NtpBackgroundDataBase {
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
    protected NtpBackgroundDataImageBase(
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
                    NtpCustomizationUtils.createThemeImageFileInDir(mFileIdHash, getImageDirName())
                            .getAbsolutePath();
        } else {
            mLastUploadImageFilePath = null;
        }
    }

    /** Returns the subdirectory name for saving the image file. */
    public abstract String getImageDirName();

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

    /** Sets the bitmap. */
    public void setBitmap(Bitmap bitmap) {
        mBitmap = bitmap;
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
    public void getBitmapOrLoadImage(Callback<@Nullable Bitmap> onImageLoadedCallback) {
        if (mBitmap != null) {
            onImageLoadedCallback.onResult(mBitmap);
            return;
        }

        NtpBackgroundDataBase currentBackgroundData =
                NtpCustomizationConfigManager.getInstance().getNtpBackgroundData();
        if (currentBackgroundData instanceof NtpBackgroundDataImageBase imageBaseData
                && Objects.equals(currentBackgroundData, this)) {
            mBitmap = imageBaseData.getBitmap();
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
        if (mPrimaryColor != null) {
            json.put(PRIMARY_COLOR_KEY, mPrimaryColor);
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
        if (obj instanceof NtpBackgroundDataImageBase other) {
            return super.equals(obj) && Objects.equals(mPrimaryColor, other.mPrimaryColor);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), mPrimaryColor);
    }

    @Override
    public @Nullable Bitmap getImageBitmapForTesting() {
        return mBitmap;
    }
}
