// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;

/**
 * Base class for NTP background data.
 *
 * <p>Two backgrounds are compared in four ways. The first two tell whether they are the same theme,
 * and leave out what daily refresh rotates. The last two tell whether they show the same picture,
 * and include it:
 *
 * <ul>
 *   <li>{@link #equals}: they are the same theme. This identifies an entry of the theme history.
 *   <li>{@link #hasSameThemeAndCompatibleColor}: they are the same theme as far as another device
 *       can tell. This decides whether a theme from another device is already the theme of this
 *       device.
 *   <li>{@link NtpBackgroundDataImageBase#hasSameImage}: they show the same image, whatever their
 *       colors. This decides whether the bitmap and the image file of one fit the other. Two theme
 *       collection backgrounds with the same image URL show the same image, even if they are
 *       different themes.
 *   <li>{@link #hasSameThemeAndColor}: they look the same right now. This decides whether applying
 *       a theme changes what the user sees.
 * </ul>
 *
 * <p>What each one compares on top of the background type. The ...CompatibleColor and ...AndColor
 * columns list what the method compares in addition to {@link #equals}. The hasSameImage column
 * lists everything it compares. "Setting" is whether daily refresh is enabled, "color" is the
 * primary color, and "+ daily refresh" is the background above with daily refresh enabled:
 *
 * <pre>
 * Background       | equals                     | ...CompatibleColor | hasSameImage | ...AndColor
 * -----------------+----------------------------+--------------------+--------------+-------------
 * Default          | nothing                    | nothing            | not an image | nothing
 * Chrome color     | setting, color ID          | nothing            | not an image | nothing
 *  + daily refresh | setting                    | nothing            | not an image | color ID
 * Color from hex   | the four colors            | nothing            | not an image | nothing
 * Uploaded image   | color, file, crop          | nothing            | like equals  | nothing
 * Theme collection | collection, setting, image | colors, if known   | image only   | color
 *  + daily refresh | collection, setting        | nothing            | image only   | image, color
 * </pre>
 *
 * <p>"Colors, if known": a null color means unknown and matches any color, e.g. iOS never sends
 * one.
 *
 * <p>Daily refresh for colors rotates between different color IDs of Chrome colors. Daily refresh
 * for theme collections rotates between images in a theme collection; on each day, the theme color
 * is derived from that day's image, and the new theme color overwrites any previous color
 * (including any color set by the user). Daily refresh cannot be on for both colors and images.
 *
 * <p>Only the daily refresh setting is synced. Each device rotates the theme on its own schedule.
 *
 * <p>None of them compares the platform a background came from or, for a theme collection, the
 * attribution, which is display-only text that differs in language and format for the same image.
 */
@NullMarked
public class NtpBackgroundDataBase {
    public static final String PORTRAIT_MATRIX_KEY = "portraitMatrix";
    public static final String LANDSCAPE_MATRIX_KEY = "landscapeMatrix";
    public static final String BACKGROUND_IMAGE_INFO_KEY = "backgroundImageInfo";

    @VisibleForTesting static final String PLATFORM_TYPE_KEY = "platformType";
    @VisibleForTesting static final String BACKGROUND_TYPE_KEY = "backgroundType";
    @VisibleForTesting static final String PRIMARY_COLOR_KEY = "primaryColor";
    @VisibleForTesting static final String FILE_ID_HASH_KEY = "fileIdHash";

    private final @PlatformType int mPlatformType;
    private @Nullable String mContentDescription;

    /**
     * @param platformType The type of platform where this NTP background data comes from.
     */
    public NtpBackgroundDataBase(@PlatformType int platformType) {
        mPlatformType = platformType;
    }

    /**
     * @param platformType The type of platform where this NTP background data comes from.
     * @param contentDescription The content description string.
     */
    public NtpBackgroundDataBase(@PlatformType int platformType, String contentDescription) {
        mPlatformType = platformType;
        mContentDescription = contentDescription;
    }

    /** Returns the platform type. */
    public @PlatformType int getPlatformType() {
        return mPlatformType;
    }

    /** Returns the NTP background type. */
    public @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.DEFAULT;
    }

    /** Returns the image drawable of this background data. */
    public @Nullable Drawable getImageDrawable() {
        return null;
    }

    /** Sets the accessibility content description for this NTP background data. */
    public void setContentDescription(@Nullable String contentDescription) {
        mContentDescription = contentDescription;
    }

    /** Returns the accessibility content description for this NTP background data. */
    public @Nullable String getContentDescription() {
        return mContentDescription;
    }

    /**
     * Gets the bitmap image and loads it asynchronously if not available
     *
     * @param onImageAvailableCallback The callback to invoke when the image is loaded.
     */
    public void getBitmapOrLoadImage(Callback<@Nullable Bitmap> onImageAvailableCallback) {}

    /** Returns the JSON representation of the object. */
    public JSONObject toJson() throws JSONException {
        JSONObject json = new JSONObject();
        json.put(PLATFORM_TYPE_KEY, mPlatformType);
        json.put(BACKGROUND_TYPE_KEY, getBackgroundType());
        return json;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj instanceof NtpBackgroundDataBase other) {
            // A theme is identified by what it looks like, not by the device it came from. The
            // sync payload carries no platform of origin, so the same background reaches this
            // device tagged ANDROID when it is replayed through sync and tagged DESKTOP or IOS
            // when it is listed as a cross device history entry. Comparing the platform would
            // make those two copies distinct and show the same background twice.
            return getBackgroundType() == other.getBackgroundType();
        }
        return false;
    }

    /**
     * Returns whether {@code obj} is the same theme and looks the same right now (see the class
     * comment). Callers use it to decide whether applying a theme changes what the user sees, e.g.
     * whether to recreate the activity.
     */
    public boolean hasSameThemeAndColor(@Nullable Object obj) {
        return equals(obj);
    }

    /**
     * Returns whether {@code obj} is the same theme as far as another device can tell (see the
     * class comment). Callers use it to decide whether a theme from another device is already the
     * theme of this device.
     *
     * <p>It only differs from {@link #equals} for a theme collection image without daily refresh:
     * the primary colors must match, unless one of them is null. A null color means unknown and
     * matches any color, e.g. iOS never sends one.
     */
    public boolean hasSameThemeAndCompatibleColor(@Nullable Object obj) {
        return equals(obj);
    }

    @Override
    public int hashCode() {
        return Integer.hashCode(getBackgroundType());
    }

    /** Returns the image bitmap of this background data. */
    public @Nullable Bitmap getImageBitmapForTesting() {
        return null;
    }
}
