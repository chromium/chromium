// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NTP_THEME_COLLECTION_IMAGES_DIR;

import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.url.GURL;

import java.util.Objects;

/** Data class for NTP theme collection background image. */
@NullMarked
public class NtpBackgroundDataThemeCollection extends NtpBackgroundDataImageBase {
    @VisibleForTesting static final String CUSTOM_BACKGROUND_INFO_KEY = "customBackgroundInfo";
    @VisibleForTesting static final String BACKGROUND_URL_KEY = "backgroundUrl";
    @VisibleForTesting static final String COLLECTION_ID_KEY = "collectionId";
    @VisibleForTesting static final String IS_UPLOADED_IMAGE_KEY = "isUploadedImage";
    @VisibleForTesting static final String IS_DAILY_REFRESH_ENABLED_KEY = "isDailyRefreshEnabled";

    private final CustomBackgroundInfo mCustomBackgroundInfo;

    private @Nullable Bitmap mPreviewBitmap;

    /**
     * Constructor.
     *
     * @param platformType The platform type of the device.
     * @param customBackgroundInfo The custom background info.
     * @param backgroundImageInfo The background image info containing matrices and window sizes.
     * @param bitmap The local bitmap, not synced.
     * @param primaryColor The primary color of the background image.
     * @param fileIdHash The hash of the background image file ID.
     */
    public NtpBackgroundDataThemeCollection(
            @PlatformType int platformType,
            CustomBackgroundInfo customBackgroundInfo,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @Nullable Bitmap bitmap,
            @Nullable @ColorInt Integer primaryColor,
            @Nullable String fileIdHash) {
        super(platformType, backgroundImageInfo, bitmap, primaryColor, fileIdHash);
        mCustomBackgroundInfo = customBackgroundInfo;
    }

    /**
     * Constructor.
     *
     * @param platformType The platform type of the device.
     * @param customBackgroundInfo The custom background info.
     * @param previewBitmap The bitmap for preview, not synced.
     */
    public NtpBackgroundDataThemeCollection(
            @PlatformType int platformType,
            CustomBackgroundInfo customBackgroundInfo,
            @Nullable Bitmap previewBitmap) {
        this(
                platformType,
                customBackgroundInfo,
                /* backgroundImageInfo= */ null,
                /* bitmap= */ null,
                /* primaryColor= */ null,
                /* fileIdHash= */ null);
        mPreviewBitmap = previewBitmap;
    }

    // NtpBackgroundDataImageBase implementations.
    @Override
    public String getImageDirName() {
        return NTP_THEME_COLLECTION_IMAGES_DIR;
    }

    @Override
    public @Nullable Bitmap getPreviewBitmap() {
        return mPreviewBitmap;
    }

    /** Returns the {@link CustomBackgroundInfo}. */
    @Override
    public CustomBackgroundInfo getCustomBackgroundInfo() {
        return mCustomBackgroundInfo;
    }

    // NtpBackgroundDataBase implementations.
    @Override
    public @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.THEME_COLLECTION;
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject json = super.toJson();
        json.put(CUSTOM_BACKGROUND_INFO_KEY, customBackgroundInfoToJson());
        return json;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof NtpBackgroundDataThemeCollection other) {
            return super.equals(obj)
                    && Objects.equals(mCustomBackgroundInfo, other.mCustomBackgroundInfo);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), mCustomBackgroundInfo);
    }

    /** Returns the NtpBackgroundDataThemeCollection object from the given JSON. */
    public static NtpBackgroundDataThemeCollection fromJson(JSONObject json) throws JSONException {
        BackgroundImageInfo backgroundImageInfo = null;
        if (json.has(BACKGROUND_IMAGE_INFO_KEY)) {
            backgroundImageInfo =
                    BackgroundImageInfo.fromJson(json.getJSONObject(BACKGROUND_IMAGE_INFO_KEY));
        }

        NtpBackgroundDataThemeCollection data =
                new NtpBackgroundDataThemeCollection(
                        json.getInt(PLATFORM_TYPE_KEY),
                        jsonObjectToCustomBackgroundInfo(
                                json.getJSONObject(CUSTOM_BACKGROUND_INFO_KEY)),
                        backgroundImageInfo,
                        /* bitmap= */ null,
                        json.has(PRIMARY_COLOR_KEY) ? json.getInt(PRIMARY_COLOR_KEY) : null,
                        json.has(FILE_ID_HASH_KEY) ? json.getString(FILE_ID_HASH_KEY) : null);
        data.setIsBitmapSavedFromJson(json);
        return data;
    }

    private static CustomBackgroundInfo jsonObjectToCustomBackgroundInfo(JSONObject json)
            throws JSONException {
        String urlSpec = json.optString(BACKGROUND_URL_KEY, null);
        GURL backgroundUrl =
                (urlSpec == null || urlSpec.isEmpty()) ? GURL.emptyGURL() : new GURL(urlSpec);
        return new CustomBackgroundInfo(
                backgroundUrl,
                json.getString(COLLECTION_ID_KEY),
                json.getBoolean(IS_UPLOADED_IMAGE_KEY),
                json.getBoolean(IS_DAILY_REFRESH_ENABLED_KEY));
    }

    private JSONObject customBackgroundInfoToJson() throws JSONException {
        JSONObject json = new JSONObject();
        json.put(
                BACKGROUND_URL_KEY,
                mCustomBackgroundInfo.backgroundUrl == null
                        ? null
                        : mCustomBackgroundInfo.backgroundUrl.getPossiblyInvalidSpec());
        json.put(COLLECTION_ID_KEY, mCustomBackgroundInfo.collectionId);
        json.put(IS_UPLOADED_IMAGE_KEY, mCustomBackgroundInfo.isUploadedImage);
        json.put(IS_DAILY_REFRESH_ENABLED_KEY, mCustomBackgroundInfo.isDailyRefreshEnabled);
        return json;
    }
}
