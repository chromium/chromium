// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;

import java.util.Objects;

/** Data class for NTP customized background color. */
@NullMarked
public class NtpBackgroundDataCustomizedColor extends NtpBackgroundDataBase {
    @VisibleForTesting static final String PRIMARY_COLOR_LIGHT_MODE_KEY = "primaryColorLight";
    @VisibleForTesting static final String PRIMARY_COLOR_DARK_MODE_KEY = "primaryColorDark";

    @VisibleForTesting
    static final String NTP_BACKGROUND_COLOR_LIGHT_MODE_KEY = "ntpBackgroundColorLight";

    @VisibleForTesting
    static final String NTP_BACKGROUND_COLOR_DARK_MODE_KEY = "ntpBackgroundColorDark";

    private final NtpThemeColorFromHexInfo mNtpThemeColorFromHexInfo;

    /**
     * @param context The application context.
     * @param platformType The type of platform where this NTP background data comes from.
     * @param primaryColorLight The primary color in light mode.
     * @param primaryColorDark The primary color in dark mode.
     * @param ntpBackgroundColorLight The background color in light mode.
     * @param ntpBackgroundColorDark The background color in dark mode.
     */
    public NtpBackgroundDataCustomizedColor(
            Context context,
            @PlatformType int platformType,
            @ColorInt int primaryColorLight,
            @ColorInt int primaryColorDark,
            @ColorInt int ntpBackgroundColorLight,
            @ColorInt int ntpBackgroundColorDark) {
        this(
                platformType,
                new NtpThemeColorFromHexInfo(
                        context,
                        ntpBackgroundColorLight,
                        ntpBackgroundColorDark,
                        primaryColorLight,
                        primaryColorDark));
    }

    /**
     * @param platformType The type of platform where this NTP background data comes from.
     * @param ntpThemeColorFromHexInfo The corresponding instance of {@link
     *     NtpThemeColorFromHexInfo}.
     */
    public NtpBackgroundDataCustomizedColor(
            @PlatformType int platformType, NtpThemeColorFromHexInfo ntpThemeColorFromHexInfo) {
        super(platformType);
        mNtpThemeColorFromHexInfo = ntpThemeColorFromHexInfo;
    }

    /** Returns the primary color in light mode. */
    public @ColorInt int getPrimaryColorLight() {
        return mNtpThemeColorFromHexInfo.primaryColorLight;
    }

    /** Returns the primary color in dark mode. */
    public @ColorInt int getPrimaryColorDark() {
        return mNtpThemeColorFromHexInfo.primaryColorDark;
    }

    /** Returns the background color in light mode. */
    public @ColorInt int getNtpBackgroundColorLight() {
        return mNtpThemeColorFromHexInfo.backgroundColorLight;
    }

    /** Returns the background color in dark mode. */
    public @ColorInt int getNtpBackgroundColorDark() {
        return mNtpThemeColorFromHexInfo.backgroundColorDark;
    }

    /** Returns the instance of {@link NtpThemeColorFromHexInfo}. */
    public NtpThemeColorFromHexInfo getNtpThemeColorFromHexInfo() {
        return mNtpThemeColorFromHexInfo;
    }

    // NtpBackgroundDataBase implementations.
    @Override
    protected @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.COLOR_FROM_HEX;
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject jsonObject = super.toJson();
        jsonObject.put(PRIMARY_COLOR_LIGHT_MODE_KEY, mNtpThemeColorFromHexInfo.primaryColorLight);
        jsonObject.put(PRIMARY_COLOR_DARK_MODE_KEY, mNtpThemeColorFromHexInfo.primaryColorDark);
        jsonObject.put(
                NTP_BACKGROUND_COLOR_LIGHT_MODE_KEY,
                mNtpThemeColorFromHexInfo.backgroundColorLight);
        jsonObject.put(
                NTP_BACKGROUND_COLOR_DARK_MODE_KEY, mNtpThemeColorFromHexInfo.backgroundColorDark);
        return jsonObject;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof NtpBackgroundDataCustomizedColor other) {
            return super.equals(obj)
                    && mNtpThemeColorFromHexInfo.equals(other.getNtpThemeColorFromHexInfo());
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(super.hashCode(), mNtpThemeColorFromHexInfo.hashCode());
    }

    /** Returns the NtpBackgroundDataCustomizedColor object from the given JSON. */
    public static NtpBackgroundDataCustomizedColor fromJson(Context context, JSONObject json)
            throws JSONException {
        return new NtpBackgroundDataCustomizedColor(
                context,
                json.getInt(PLATFORM_TYPE_KEY),
                json.getInt(PRIMARY_COLOR_LIGHT_MODE_KEY),
                json.getInt(PRIMARY_COLOR_DARK_MODE_KEY),
                json.getInt(NTP_BACKGROUND_COLOR_LIGHT_MODE_KEY),
                json.getInt(NTP_BACKGROUND_COLOR_DARK_MODE_KEY));
    }
}
