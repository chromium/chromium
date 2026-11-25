// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Objects;

/** A class to hold information about a custom background. */
@NullMarked
public class CustomBackgroundInfo {
    /** The version of a {@link CustomBackgroundInfo} string stored in SharedPreferences. */
    @IntDef({CustomBackgroundInfoVersion.LEGACY, CustomBackgroundInfoVersion.JSON_V1})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CustomBackgroundInfoVersion {
        // The old format: "backgroundUrl|collectionId|isUploadedImage|isDailyRefreshEnabled".
        // It only has these four fields. Chrome can still read it, but no longer writes it.
        int LEGACY = 0;
        // JSON with all fields of CustomBackgroundInfo, plus the version number.
        int JSON_V1 = 1;
    }

    public static final String VERSION_KEY = "version";
    private static final String BACKGROUND_URL_KEY = "backgroundUrl";
    private static final String COLLECTION_ID_KEY = "collectionId";
    private static final String IS_UPLOADED_IMAGE_KEY = "isUploadedImage";
    private static final String IS_DAILY_REFRESH_ENABLED_KEY = "isDailyRefreshEnabled";
    private static final String ATTRIBUTION_LINE_1_KEY = "attributionLine1";
    private static final String ATTRIBUTION_LINE_2_KEY = "attributionLine2";

    @VisibleForTesting
    public static final String ATTRIBUTION_ACTION_URL_KEY = "attributionActionUrl";

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
     * The URL the attribution links to, as read from and written to the synced background. It is
     * {@link GURL#emptyGURL()} when the image has no such link.
     *
     * <p>Like the attribution lines, it describes the image rather than identifies it, so it is
     * deliberately not part of {@link #equals} either.
     */
    public final GURL attributionActionUrl;

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
                /* attributionLine2= */ null,
                GURL.emptyGURL());
    }

    /**
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     * @param attributionLine1 The first attribution line of the background image.
     * @param attributionLine2 The second attribution line of the background image.
     * @param attributionActionUrl The URL the attribution links to, or {@link GURL#emptyGURL()}.
     */
    public CustomBackgroundInfo(
            GURL backgroundUrl,
            String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled,
            @Nullable String attributionLine1,
            @Nullable String attributionLine2,
            GURL attributionActionUrl) {
        this.backgroundUrl = backgroundUrl;
        this.collectionId = collectionId;
        this.isUploadedImage = isUploadedImage;
        this.isDailyRefreshEnabled = isDailyRefreshEnabled;
        this.attributionLine1 = attributionLine1;
        this.attributionLine2 = attributionLine2;
        this.attributionActionUrl = attributionActionUrl;
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
     * @param attributionActionUrl The URL the attribution links to, or {@link GURL#emptyGURL()}.
     */
    public static CustomBackgroundInfo createCustomBackgroundInfo(
            GURL backgroundUrl,
            String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled,
            List<String> attributions,
            GURL attributionActionUrl) {
        return new CustomBackgroundInfo(
                backgroundUrl,
                collectionId,
                isUploadedImage,
                isDailyRefreshEnabled,
                attributions != null && attributions.size() > 0 ? attributions.get(0) : null,
                attributions != null && attributions.size() > 1 ? attributions.get(1) : null,
                attributionActionUrl);
    }

    /**
     * Returns the JSON representation of the object, which includes the version. Attributions that
     * aren't set are left out.
     */
    public JSONObject toJson() throws JSONException {
        JSONObject json = new JSONObject();
        json.put(VERSION_KEY, CustomBackgroundInfoVersion.JSON_V1);
        json.put(
                BACKGROUND_URL_KEY,
                backgroundUrl == null ? null : backgroundUrl.getPossiblyInvalidSpec());
        json.put(COLLECTION_ID_KEY, collectionId);
        json.put(IS_UPLOADED_IMAGE_KEY, isUploadedImage);
        json.put(IS_DAILY_REFRESH_ENABLED_KEY, isDailyRefreshEnabled);
        if (attributionLine1 != null) {
            json.put(ATTRIBUTION_LINE_1_KEY, attributionLine1);
        }
        if (attributionLine2 != null) {
            json.put(ATTRIBUTION_LINE_2_KEY, attributionLine2);
        }
        if (!attributionActionUrl.isEmpty()) {
            json.put(ATTRIBUTION_ACTION_URL_KEY, attributionActionUrl.getPossiblyInvalidSpec());
        }
        return json;
    }

    /** Returns the CustomBackgroundInfo object from the given JSON. */
    public static CustomBackgroundInfo fromJson(JSONObject json) throws JSONException {
        String attributionLine1 =
                json.has(ATTRIBUTION_LINE_1_KEY) ? json.getString(ATTRIBUTION_LINE_1_KEY) : null;
        String attributionLine2 =
                json.has(ATTRIBUTION_LINE_2_KEY) ? json.getString(ATTRIBUTION_LINE_2_KEY) : null;
        return new CustomBackgroundInfo(
                getGurlFromJson(json, BACKGROUND_URL_KEY),
                json.getString(COLLECTION_ID_KEY),
                json.getBoolean(IS_UPLOADED_IMAGE_KEY),
                json.getBoolean(IS_DAILY_REFRESH_ENABLED_KEY),
                attributionLine1,
                attributionLine2,
                getGurlFromJson(json, ATTRIBUTION_ACTION_URL_KEY));
    }

    /** Returns the GURL stored under the key, or {@link GURL#emptyGURL()} if missing or empty. */
    private static GURL getGurlFromJson(JSONObject json, String key) {
        String spec = json.optString(key);
        return spec.isEmpty() ? GURL.emptyGURL() : new GURL(spec);
    }

    /**
     * Compares the fields that identify the theme. The attribution lines and the attribution action
     * URL are deliberately left out, see {@link #attributionLine1} and {@link
     * #attributionActionUrl}.
     *
     * <p>A theme collection with daily refresh is identified by the collection rather than by the
     * image: each device rotates through the collection on its own schedule and only the setting is
     * synced, so two such infos are the same theme whichever image each device currently shows. The
     * daily refresh setting itself must still match, as turning it on or off is a change of theme.
     */
    @Override
    public boolean equals(@Nullable Object obj) {
        if (!(obj instanceof CustomBackgroundInfo other)) return false;

        if (!Objects.equals(collectionId, other.collectionId)
                || isUploadedImage != other.isUploadedImage
                || isDailyRefreshEnabled != other.isDailyRefreshEnabled) {
            return false;
        }

        return isDailyRefreshEnabled || Objects.equals(backgroundUrl, other.backgroundUrl);
    }

    @Override
    public int hashCode() {
        // equals() ignores the URL of a daily refresh theme collection, so hashCode() must too.
        return Objects.hash(
                isDailyRefreshEnabled ? null : backgroundUrl,
                collectionId,
                isUploadedImage,
                isDailyRefreshEnabled);
    }
}
