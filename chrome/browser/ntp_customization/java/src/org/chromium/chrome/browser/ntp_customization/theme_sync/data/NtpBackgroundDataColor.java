// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

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
    NtpBackgroundDataColor(
            @PlatformType int platformType,
            boolean isChromeColorDailyRefreshEnabled,
            NtpThemeColorInfo ntpThemeColorInfo) {
        super(platformType);
        mIsChromeColorDailyRefreshEnabled = isChromeColorDailyRefreshEnabled;
        mNtpThemeColorInfo = ntpThemeColorInfo;
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
    public @Nullable NtpThemeColorInfo getNtpThemeColorInfo() {
        return mNtpThemeColorInfo;
    }

    // NtpBackgroundDataBase implementations.
    @Override
    protected @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.CHROME_COLOR;
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject jsonObject = super.toJson();
        jsonObject.put(THEME_COLOR_ID_KEY, getThemeColorId());
        jsonObject.put(IS_DAILY_REFRESH_ENABLED_KEY, mIsChromeColorDailyRefreshEnabled);
        return jsonObject;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof NtpBackgroundDataColor other) {
            return super.equals(obj) && getThemeColorId() == other.getThemeColorId();
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), getThemeColorId());
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
