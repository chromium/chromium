// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;

/** A class to hold information about a custom background. */
@NullMarked
public class CustomBackgroundInfo {
    public final GURL backgroundUrl;
    public final String collectionId;
    public final boolean isUploadedImage;
    public final boolean isDailyRefreshEnabled;

    /**
     * The human readable attribution of the background image.
     *
     * <p>This is a display string that the backdrop server localizes to the UI language of the
     * device that requested it, so two devices running in different languages describe the same
     * image differently, and sync hands the string over verbatim. It is therefore deliberately not
     * part of {@link #equals}: comparing it would make the very same image look like two different
     * ones and duplicate it in the theme history.
     */
    public final @Nullable String attribution;

    /**
     * TODO(https://crbug.com/488439751): Cleans up this method.
     *
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     */
    @Deprecated
    public CustomBackgroundInfo(
            GURL backgroundUrl,
            String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled) {
        this(
                backgroundUrl,
                collectionId,
                isUploadedImage,
                isDailyRefreshEnabled,
                /* attribution= */ null);
    }

    /**
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     * @param attribution The attribution of the background image.
     */
    public CustomBackgroundInfo(
            GURL backgroundUrl,
            String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled,
            @Nullable String attribution) {
        this.backgroundUrl = backgroundUrl;
        this.collectionId = collectionId;
        this.isUploadedImage = isUploadedImage;
        this.isDailyRefreshEnabled = isDailyRefreshEnabled;
        this.attribution = attribution;
    }

    /**
     * A helper function to create a CustomBackgroundInfo with a list of attributions.
     *
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     * @param attributions A list of attributions of the background image.
     */
    public static CustomBackgroundInfo createCustomBackgroundInfo(
            GURL backgroundUrl,
            String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled,
            List<String> attributions) {
        return new CustomBackgroundInfo(
                backgroundUrl,
                collectionId,
                isUploadedImage,
                isDailyRefreshEnabled,
                attributions != null ? String.join(", ", attributions) : null);
    }

    /**
     * Compares the fields that identify the image itself. The attribution is deliberately left out,
     * see {@link #attribution}.
     */
    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj instanceof CustomBackgroundInfo other) {
            return Objects.equals(backgroundUrl, other.backgroundUrl)
                    && Objects.equals(collectionId, other.collectionId)
                    && isUploadedImage == other.isUploadedImage
                    && isDailyRefreshEnabled == other.isDailyRefreshEnabled;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(backgroundUrl, collectionId, isUploadedImage, isDailyRefreshEnabled);
    }
}
