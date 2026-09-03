// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NTP_UPLOAD_IMAGES_DIR;

import android.graphics.Bitmap;

import androidx.annotation.ColorInt;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;

import java.util.Objects;

/** Data class for NTP uploaded background image. */
@NullMarked
public class NtpBackgroundDataUploadImage extends NtpBackgroundDataImageBase {
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
        super(platformType, backgroundImageInfo, bitmap, primaryColor, fileIdHash);
    }

    /** Returns the subdirectory name for saving the image file. */
    @Override
    public String getImageDirName() {
        return NTP_UPLOAD_IMAGES_DIR;
    }

    // NtpBackgroundDataBase implementations.

    @Override
    public @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.IMAGE_FROM_DISK;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj instanceof NtpBackgroundDataUploadImage other) {
            return super.equals(obj)
                    && Objects.equals(getFileIdHash(), other.getFileIdHash())
                    && Objects.equals(getBackgroundImageInfo(), other.getBackgroundImageInfo());
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), getFileIdHash(), getBackgroundImageInfo());
    }

    /** Returns the NtpBackgroundDataUploadImage object from the given JSON. */
    public static NtpBackgroundDataUploadImage fromJson(JSONObject json) throws JSONException {
        BackgroundImageInfo backgroundImageInfo = null;
        if (json.has(BACKGROUND_IMAGE_INFO_KEY)) {
            backgroundImageInfo =
                    BackgroundImageInfo.fromJson(json.getJSONObject(BACKGROUND_IMAGE_INFO_KEY));
        }
        NtpBackgroundDataUploadImage data =
                new NtpBackgroundDataUploadImage(
                        json.getInt(PLATFORM_TYPE_KEY),
                        backgroundImageInfo,
                        /* bitmap= */ null,
                        json.has(PRIMARY_COLOR_KEY) ? json.getInt(PRIMARY_COLOR_KEY) : null,
                        json.has(FILE_ID_HASH_KEY) ? json.getString(FILE_ID_HASH_KEY) : null);
        data.setIsBitmapSavedFromJson(json);
        return data;
    }
}
