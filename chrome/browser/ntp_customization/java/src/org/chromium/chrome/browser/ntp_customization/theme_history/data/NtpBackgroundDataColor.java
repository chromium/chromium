// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;

/** Data class for NTP background color. */
@NullMarked
public class NtpBackgroundDataColor extends NtpBackgroundDataBase {
    @VisibleForTesting static final String THEME_COLOR_ID_KEY = "theme_color_id";

    @VisibleForTesting
    static final String IS_DAILY_REFRESH_ENABLED_KEY = "isChromeColorDailyRefreshEnabled";

    private final @NtpThemeColorId int mThemeColorId;
    private final boolean mIsChromeColorDailyRefreshEnabled;

    public NtpBackgroundDataColor(
            @PlatformType int platformType,
            @NtpThemeColorId int themeColorId,
            boolean isChromeColorDailyRefreshEnabled) {
        super(platformType);
        mThemeColorId = themeColorId;
        mIsChromeColorDailyRefreshEnabled = isChromeColorDailyRefreshEnabled;
    }

    /** Returns the NTP theme color ID. */
    public @NtpThemeColorId int getThemeColorId() {
        return mThemeColorId;
    }

    /** Returns whether Chrome color daily refresh is enabled. */
    public boolean isChromeColorDailyRefreshEnabled() {
        return mIsChromeColorDailyRefreshEnabled;
    }

    // NtpBackgroundDataBase implementations.

    @Override
    protected @NtpBackgroundType int getBackgroundType() {
        return NtpBackgroundType.CHROME_COLOR;
    }

    @Override
    public JSONObject toJson() throws JSONException {
        JSONObject jsonObject = super.toJson();
        jsonObject.put(THEME_COLOR_ID_KEY, mThemeColorId);
        jsonObject.put(IS_DAILY_REFRESH_ENABLED_KEY, mIsChromeColorDailyRefreshEnabled);
        return jsonObject;
    }

    /** Returns the NtpBackgroundDataColor object from the given JSON representation. */
    public static NtpBackgroundDataColor fromJson(JSONObject json) throws JSONException {
        return new NtpBackgroundDataColor(
                json.getInt(PLATFORM_TYPE_KEY),
                json.getInt(THEME_COLOR_ID_KEY),
                json.getBoolean(IS_DAILY_REFRESH_ENABLED_KEY));
    }
}
