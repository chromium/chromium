// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;

import java.util.Objects;

/** Data class for NTP background color. */
@NullMarked
public class NtpBackgroundDataColor extends NtpBackgroundDataBase {
    @VisibleForTesting static final String THEME_COLOR_ID_KEY = "theme_color_id";

    @VisibleForTesting
    static final String IS_DAILY_REFRESH_ENABLED_KEY = "isChromeColorDailyRefreshEnabled";

    private final NtpThemeColorInfo mNtpThemeColorInfo;
    private final boolean mIsChromeColorDailyRefreshEnabled;

    /**
     * @param platformType The type of platform where this NTP background data comes from.
     * @param isChromeColorDailyRefreshEnabled Whether daily refresh of chrome color is enabled.
     * @param ntpThemeColorInfo The corresponding instance of {@link NtpThemeColorInfo}.
     */
    public NtpBackgroundDataColor(
            @PlatformType int platformType,
            boolean isChromeColorDailyRefreshEnabled,
            NtpThemeColorInfo ntpThemeColorInfo) {
        super(platformType);
        mIsChromeColorDailyRefreshEnabled = isChromeColorDailyRefreshEnabled;
        mNtpThemeColorInfo = ntpThemeColorInfo;
        setContentDescription(mNtpThemeColorInfo.colorName);
    }

    /**
     * @param context The application context.
     * @param platformType The type of platform where this NTP background data comes from.
     * @param themeColorId The color theme id.
     * @param isChromeColorDailyRefreshEnabled Whether daily refresh of chrome color is enabled.
     */
    public NtpBackgroundDataColor(
            Context context,
            @PlatformType int platformType,
            @NtpThemeColorId int themeColorId,
            boolean isChromeColorDailyRefreshEnabled) {
        this(
                platformType,
                isChromeColorDailyRefreshEnabled,
                assumeNonNull(NtpThemeColorUtils.createNtpThemeColorInfo(context, themeColorId)));
    }

    /** Returns the NTP theme color ID. */
    public @NtpThemeColorId int getThemeColorId() {
        return mNtpThemeColorInfo.id;
    }

    /** Returns whether Chrome color daily refresh is enabled. */
    public boolean isChromeColorDailyRefreshEnabled() {
        return mIsChromeColorDailyRefreshEnabled;
    }

    /** Returns the {@link NtpThemeColorInfo} instance. */
    public NtpThemeColorInfo getNtpThemeColorInfo() {
        return mNtpThemeColorInfo;
    }

    // NtpBackgroundDataBase implementations.
    @Override
    public @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.CHROME_COLOR;
    }

    @Override
    public @Nullable Drawable getImageDrawable() {
        return mNtpThemeColorInfo.iconDrawable;
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject jsonObject = super.toJson();
        jsonObject.put(THEME_COLOR_ID_KEY, getThemeColorId());
        jsonObject.put(IS_DAILY_REFRESH_ENABLED_KEY, mIsChromeColorDailyRefreshEnabled);
        return jsonObject;
    }

    /**
     * Compares the fields that identify the Chrome color theme.
     *
     * <p>A Chrome color with daily refresh rotates its color ID locally on each device on its own
     * schedule and only the setting is synced, so any two Chrome colors with daily refresh enabled
     * are the same theme whichever color each device currently shows, like in {@code
     * NtpCustomizationUtils#isSyncedBackgroundPersisted}. The daily refresh setting itself must
     * still match, as turning it on or off is a change of theme.
     */
    @Override
    public boolean equals(@Nullable Object obj) {
        if (!(obj instanceof NtpBackgroundDataColor other) || !super.equals(obj)) {
            return false;
        }

        if (mIsChromeColorDailyRefreshEnabled != other.mIsChromeColorDailyRefreshEnabled) {
            return false;
        }

        return mIsChromeColorDailyRefreshEnabled || getThemeColorId() == other.getThemeColorId();
    }

    /**
     * Unlike {@link #equals}, still compares the color ID: a rotated color ID of a Chrome color
     * with daily refresh is a visible change even though it is the same theme.
     */
    @Override
    public boolean hasSameThemeAndColor(@Nullable Object obj) {
        return obj instanceof NtpBackgroundDataColor other
                && equals(obj)
                && getThemeColorId() == other.getThemeColorId();
    }

    @Override
    public int hashCode() {
        // Follows equals(): the color ID only counts for a Chrome color without daily refresh.
        return Objects.hash(
                super.hashCode(),
                mIsChromeColorDailyRefreshEnabled,
                mIsChromeColorDailyRefreshEnabled ? 0 : getThemeColorId());
    }

    /** Returns the NtpBackgroundDataColor object from the given JSON representation. */
    public static NtpBackgroundDataColor fromJson(Context context, JSONObject json)
            throws JSONException {
        return new NtpBackgroundDataColor(
                context,
                json.getInt(PLATFORM_TYPE_KEY),
                json.getInt(THEME_COLOR_ID_KEY),
                json.getBoolean(IS_DAILY_REFRESH_ENABLED_KEY));
    }
}
