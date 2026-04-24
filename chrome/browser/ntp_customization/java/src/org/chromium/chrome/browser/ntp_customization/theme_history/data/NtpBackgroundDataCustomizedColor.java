// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;

/** Data class for NTP customized background color. */
@NullMarked
public class NtpBackgroundDataCustomizedColor extends NtpBackgroundDataBase {
    @VisibleForTesting static final String LIGHT_MODE_COLOR_KEY = "lightModeColor";
    @VisibleForTesting static final String DARK_MODE_COLOR_KEY = "darkModeColor";

    private final @ColorInt int mLightModeColor;
    private final @ColorInt int mDarkModeColor;

    public NtpBackgroundDataCustomizedColor(
            @PlatformType int platformType,
            @ColorInt int lightModeColor,
            @ColorInt int darkModeColor) {
        super(platformType);
        mLightModeColor = lightModeColor;
        mDarkModeColor = darkModeColor;
    }

    /** Returns the color in light mode. */
    public @ColorInt int getLightModeColor() {
        return mLightModeColor;
    }

    /** Returns the color in dark mode. */
    public @ColorInt int getDarkModeColor() {
        return mDarkModeColor;
    }

    // NtpBackgroundDataBase implementations.
    @Override
    protected @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.COLOR_FROM_HEX;
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject jsonObject = super.toJson();
        jsonObject.put(LIGHT_MODE_COLOR_KEY, mLightModeColor);
        jsonObject.put(DARK_MODE_COLOR_KEY, mDarkModeColor);
        return jsonObject;
    }

    /** Returns the NtpBackgroundDataCustomizedColor object from the given JSON. */
    public static NtpBackgroundDataCustomizedColor fromJson(JSONObject json) throws JSONException {
        return new NtpBackgroundDataCustomizedColor(
                json.getInt(PLATFORM_TYPE_KEY),
                json.getInt(LIGHT_MODE_COLOR_KEY),
                json.getInt(DARK_MODE_COLOR_KEY));
    }
}
