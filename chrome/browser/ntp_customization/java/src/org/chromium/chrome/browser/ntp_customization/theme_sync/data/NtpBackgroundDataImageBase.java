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
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;

import java.util.Objects;

/** Base data class for NTP image-based background data. */
@NullMarked
public abstract class NtpBackgroundDataImageBase extends NtpBackgroundDataBase {
    @VisibleForTesting static final String IS_BITMAP_SAVED_KEY = "isBitmapSaved";

    private @Nullable BackgroundImageInfo mBackgroundImageInfo;
    // The mFileIdHash isn't null when NTP theme sync is enabled.
    private @Nullable String mFileIdHash;
    // The mLastUploadImageFilePath isn't null when mFileIdHash isn't null.
    private @Nullable String mLastUploadImageFilePath;
    private @Nullable Bitmap mBitmap;
    private boolean mIsBitmapSaved;
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

        setFileIdHash(fileIdHash);
    }

    /** Returns the subdirectory name for saving the image file. */
    public abstract String getImageDirName();

    /** Returns the bitmap for preview, or null if not available. */
    public @Nullable Bitmap getPreviewBitmap() {
        return null;
    }

    /** Returns the file path of the last uploaded image. */
    public @Nullable String getLastUploadImageFilePath() {
        return mLastUploadImageFilePath;
    }

    /** Returns the background image info containing matrices and window sizes. */
    public @Nullable BackgroundImageInfo getBackgroundImageInfo() {
        return mBackgroundImageInfo;
    }

    /** Sets the background image info containing matrices and window sizes. */
    public void setBackgroundImageInfo(@Nullable BackgroundImageInfo info) {
        mBackgroundImageInfo = info;
    }

    /** Returns the local bitmap, which is not synced. */
    public @Nullable Bitmap getBitmap() {
        return mBitmap;
    }

    /** Sets the bitmap. */
    public void setBitmap(@Nullable Bitmap bitmap) {
        mBitmap = bitmap;
    }

    /** Returns whether the bitmap has been saved to the device as a file. */
    public boolean isBitmapSaved() {
        return mIsBitmapSaved;
    }

    /**
     * Sets whether the bitmap has been saved to the device as a file.
     *
     * @param isBitmapSaved Whether the bitmap has been saved.
     */
    public void setIsBitmapSaved(boolean isBitmapSaved) {
        mIsBitmapSaved = isBitmapSaved;
    }

    /** Returns the {@link CustomBackgroundInfo} if it's a theme collection, or null otherwise. */
    public @Nullable CustomBackgroundInfo getCustomBackgroundInfo() {
        return null;
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

    /**
     * Sets the file ID hash of the background image and updates the last upload image file path.
     *
     * @param fileIdHash The file ID hash to set.
     */
    public void setFileIdHash(@Nullable String fileIdHash) {
        mFileIdHash = fileIdHash;
        if (mFileIdHash != null) {
            mLastUploadImageFilePath =
                    NtpCustomizationUtils.createThemeImageFileInDir(mFileIdHash, getImageDirName())
                            .getAbsolutePath();
        } else {
            mLastUploadImageFilePath = null;
        }
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
        } else if (getPreviewBitmap() != null) {
            onImageLoadedCallback.onResult(getPreviewBitmap());
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
        json.put(IS_BITMAP_SAVED_KEY, mIsBitmapSaved);
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

    /** Reads the isBitmapSaved value from the given JSON and sets it on this object. */
    public void setIsBitmapSavedFromJson(JSONObject json) {
        setIsBitmapSaved(json.optBoolean(IS_BITMAP_SAVED_KEY, false));
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
