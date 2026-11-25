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

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Data class for NTP theme collection background image. */
@NullMarked
public class NtpBackgroundDataThemeCollection extends NtpBackgroundDataImageBase {
    @VisibleForTesting static final String CUSTOM_BACKGROUND_INFO_KEY = "customBackgroundInfo";

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
        // Set even when empty, which clears a stale description from a recycled view.
        setContentDescription(
                joinAttributionLines(
                        customBackgroundInfo.attributionLine1,
                        customBackgroundInfo.attributionLine2));
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

    /**
     * Returns whether {@code other} shows the same image, which means it has the same image URL.
     * The theme isn't compared. With daily refresh, one theme shows different images over time. A
     * static image and a daily refresh collection can also show the same image. Two backgrounds
     * without an image URL don't show the same image.
     */
    @Override
    public boolean hasSameImage(@Nullable NtpBackgroundDataBase other) {
        return other instanceof NtpBackgroundDataThemeCollection otherCollection
                && !mCustomBackgroundInfo.backgroundUrl.isEmpty()
                && Objects.equals(
                        mCustomBackgroundInfo.backgroundUrl,
                        otherCollection.mCustomBackgroundInfo.backgroundUrl);
    }

    // NtpBackgroundDataBase implementations.
    @Override
    public @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.THEME_COLLECTION;
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject json = super.toJson();
        json.put(CUSTOM_BACKGROUND_INFO_KEY, mCustomBackgroundInfo.toJson());
        return json;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj instanceof NtpBackgroundDataThemeCollection other) {
            return super.equals(obj)
                    && Objects.equals(mCustomBackgroundInfo, other.mCustomBackgroundInfo);
        }
        return false;
    }

    @Override
    public boolean isIdentical(@Nullable NtpBackgroundDataBase other) {
        // Two backgrounds look the same only if they are the same theme and show the same image
        // with the same color. With daily refresh, one collection shows different images over
        // time, and those look different.
        return other instanceof NtpBackgroundDataThemeCollection otherCollection
                && equals(otherCollection)
                && Objects.equals(
                        mCustomBackgroundInfo.backgroundUrl,
                        otherCollection.mCustomBackgroundInfo.backgroundUrl)
                && Objects.equals(getPrimaryColor(), otherCollection.getPrimaryColor());
    }

    @Override
    public boolean isSameThemeAcrossDevices(@Nullable NtpBackgroundDataBase other) {
        if (!(other instanceof NtpBackgroundDataThemeCollection otherCollection)
                || !equals(other)) {
            return false;
        }

        if (mCustomBackgroundInfo.isDailyRefreshEnabled) return true;

        @Nullable
        @ColorInt
        Integer color = getPrimaryColor();
        @Nullable
        @ColorInt
        Integer otherColor = otherCollection.getPrimaryColor();
        return color == null || otherColor == null || color.equals(otherColor);
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
                        CustomBackgroundInfo.fromJson(
                                json.getJSONObject(CUSTOM_BACKGROUND_INFO_KEY)),
                        backgroundImageInfo,
                        /* bitmap= */ null,
                        json.has(PRIMARY_COLOR_KEY) ? json.getInt(PRIMARY_COLOR_KEY) : null,
                        json.has(FILE_ID_HASH_KEY) ? json.getString(FILE_ID_HASH_KEY) : null);
        data.setIsBitmapSavedFromJson(json);
        return data;
    }

    /** Joins the non-empty attribution lines with a comma and a space. */
    private static String joinAttributionLines(
            @Nullable String attributionLine1, @Nullable String attributionLine2) {
        List<String> lines = new ArrayList<>();
        if (attributionLine1 != null && !attributionLine1.isEmpty()) {
            lines.add(attributionLine1);
        }
        if (attributionLine2 != null && !attributionLine2.isEmpty()) {
            lines.add(attributionLine2);
        }
        return String.join(", ", lines);
    }
}
