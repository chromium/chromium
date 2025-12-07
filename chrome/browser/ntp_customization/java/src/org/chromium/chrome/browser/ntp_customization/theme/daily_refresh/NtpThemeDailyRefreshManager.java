// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.daily_refresh;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;

@NullMarked
/** Handles the daily refresh of a customized NTP's background color or image. */
public class NtpThemeDailyRefreshManager {
    private static @Nullable NtpThemeDailyRefreshManager sInstanceForTesting;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final NtpThemeDailyRefreshManager sInstance = new NtpThemeDailyRefreshManager();
    }

    private boolean mIsDailyUpdateApplied;
    private @Nullable Long mLastDailyUpdateTimestamp;
    private @Nullable @NtpThemeColorId Integer mNtpThemeColorId;

    /** Returns the singleton instance of NtpThemeDailyUpdateManager. */
    public static NtpThemeDailyRefreshManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return NtpThemeDailyRefreshManager.LazyHolder.sInstance;
    }

    @VisibleForTesting
    public NtpThemeDailyRefreshManager() {}

    /**
     * Get the updated NtpThemeColorId if daily refresh is applied, otherwise the color ID saved in
     * the SharedPreference. This method should be called if the NTP theme type is Chrome Color.
     */
    public @NtpThemeColorId int getNtpThemeColorIdForChromeColorTheme() {
        if (mIsDailyUpdateApplied && mNtpThemeColorId != null) {
            return mNtpThemeColorId;
        }

        return NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference();
    }

    /**
     * Returns the next color ID if daily refresh is enabled and hasn't been updated within 24
     * hours.
     *
     * @param currentColorId The current color ID.
     */
    @VisibleForTesting
    public @NtpThemeColorId int mayApplyDailyRefreshForChromeColor(
            @NtpThemeColorId int currentColorId) {
        if (!isDailyRefreshEnabled(NtpBackgroundImageType.CHROME_COLOR)) {
            return currentColorId;
        }

        return applyDailyRefresh(currentColorId);
    }

    @VisibleForTesting
    boolean isDailyRefreshEnabled(@NtpBackgroundImageType int backgroundType) {
        if (backgroundType != NtpBackgroundImageType.CHROME_COLOR) {
            return false;
        }

        if (!NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference()) {
            return false;
        }

        long dailyRefreshHoursMs =
                ChromeFeatureList.sNewTabPageCustomizationV2DailyRefreshThresholdMs.getValue();

        if (mLastDailyUpdateTimestamp == null) {
            mLastDailyUpdateTimestamp =
                    NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference();
        }

        return TimeUtils.currentTimeMillis() - mLastDailyUpdateTimestamp > dailyRefreshHoursMs;
    }

    @NtpThemeColorId
    int applyDailyRefresh(@NtpThemeColorId int currentColorId) {
        @NtpThemeColorId int newColorId = getNextColorId(currentColorId);
        setDailyUpdateStatus(newColorId, TimeUtils.currentTimeMillis());
        return newColorId;
    }

    @VisibleForTesting
    static @NtpThemeColorId int getNextColorId(@NtpThemeColorId int currentColorId) {
        @NtpThemeColorId int newColor = currentColorId + 1;
        if (newColor == NtpThemeColorId.NUM_ENTRIES) {
            newColor = NtpThemeColorId.DEFAULT + 1;
        }
        return newColor;
    }

    @VisibleForTesting
    void setDailyUpdateStatus(@NtpThemeColorId int newColorId, long timestamp) {
        mNtpThemeColorId = newColorId;
        mLastDailyUpdateTimestamp = timestamp;
        mIsDailyUpdateApplied = true;
    }

    /** Saves the existing daily refresh settings to the SharedPreference and resets. */
    public void maybeSaveDailyRefreshAndReset() {
        if (!mIsDailyUpdateApplied || !ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled()) {
            return;
        }

        if (mNtpThemeColorId != null) {
            NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(mNtpThemeColorId);
            mNtpThemeColorId = null;
        }

        if (mLastDailyUpdateTimestamp != null) {
            NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(
                    mLastDailyUpdateTimestamp);
            mLastDailyUpdateTimestamp = null;
        }

        mIsDailyUpdateApplied = false;
    }

    public static NtpThemeDailyRefreshManager createInstanceForTesting() {
        NtpThemeDailyRefreshManager instance = new NtpThemeDailyRefreshManager();
        setInstanceForTesting(instance);
        return instance;
    }

    public static void setInstanceForTesting(NtpThemeDailyRefreshManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public boolean getIsDailyUpdateAppliedForTesting() {
        return mIsDailyUpdateApplied;
    }

    public @Nullable Long getLastDailyUpdateTimestampForTesting() {
        return mLastDailyUpdateTimestamp;
    }

    public @Nullable @NtpThemeColorId Integer getNtpThemeColorIdForTesting() {
        return mNtpThemeColorId;
    }
}
