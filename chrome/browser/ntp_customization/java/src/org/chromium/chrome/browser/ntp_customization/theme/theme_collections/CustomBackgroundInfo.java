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
     * The two human readable attribution lines of the background image, as read from and written to
     * the synced background.
     *
     * <p>These are display strings that the backdrop server localizes to the UI language of the
     * device that requested them, so two devices running in different languages describe the same
     * image differently, and sync hands the strings over verbatim. They are therefore deliberately
     * not part of {@link #equals}: comparing them would make the very same image look like two
     * different ones and duplicate it in the theme history.
     */
    public final @Nullable String attributionLine1;

    /** See {@link #attributionLine1}. */
    public final @Nullable String attributionLine2;

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
                /* attributionLine1= */ null,
                /* attributionLine2= */ null);
    }

    /**
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     * @param attributionLine1 The first attribution line of the background image.
     * @param attributionLine2 The second attribution line of the background image.
     */
    public CustomBackgroundInfo(
            GURL backgroundUrl,
            String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled,
            @Nullable String attributionLine1,
            @Nullable String attributionLine2) {
        this.backgroundUrl = backgroundUrl;
        this.collectionId = collectionId;
        this.isUploadedImage = isUploadedImage;
        this.isDailyRefreshEnabled = isDailyRefreshEnabled;
        this.attributionLine1 = attributionLine1;
        this.attributionLine2 = attributionLine2;
    }

    /**
     * A helper function to create a CustomBackgroundInfo with a list of attributions.
     *
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     * @param attributions A list of attributions of the background image. Only the first two lines
     *     are used, as Chrome theme storage and sync only support two attribution lines.
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
                attributions != null && attributions.size() > 0 ? attributions.get(0) : null,
                attributions != null && attributions.size() > 1 ? attributions.get(1) : null);
    }

    /**
     * Compares the fields that identify the image itself. The attribution lines are deliberately
     * left out, see {@link #attributionLine1}.
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
