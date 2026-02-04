// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.daily_refresh;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;

import java.util.concurrent.Executor;

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
     * Get the primary theme color for a theme collection. If a daily refresh has been applied, it
     * retrieves the color for the refreshed theme.
     */
    public @ColorInt int getNtpThemeColorForThemeCollection() {
        if (mIsDailyUpdateApplied) {
            return NtpCustomizationUtils
                    .getDailyRefreshCustomizedPrimaryColorFromSharedPreference();
        }

        return NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference();
    }

    /**
     * Gets the background image info for a theme collection. If a daily refresh has been applied,
     * it retrieves the info for the refreshed theme's image.
     */
    public @Nullable BackgroundImageInfo getNtpBackgroundImageInfoForThemeCollection() {
        if (mIsDailyUpdateApplied) {
            return NtpCustomizationUtils.readDailyRefreshNtpBackgroundImageInfo();
        }

        return NtpCustomizationUtils.readNtpBackgroundImageInfo();
    }

    /**
     * Reads the background image bitmap for a theme collection. If a daily refresh has been
     * applied, it reads the image for the refreshed theme.
     */
    public void readNtpBackgroundImageForThemeCollection(
            Callback<@Nullable Bitmap> callback, Executor executor) {
        if (mIsDailyUpdateApplied) {
            NtpCustomizationUtils.readDailyRefreshNtpBackgroundImage(callback, executor);
            return;
        }

        NtpCustomizationUtils.readNtpBackgroundImage(callback, executor);
    }

    /**
     * Gets the custom background info for a theme collection. If a daily refresh has been applied,
     * it retrieves the info for the refreshed theme.
     */
    public @Nullable CustomBackgroundInfo getNtpCustomBackgroundInfoForThemeCollection() {
        if (mIsDailyUpdateApplied) {
            return NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference();
        }

        return NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference();
    }

    /**
     * Returns the next color ID if daily refresh is enabled and hasn't been updated within 24
     * hours.
     *
     * @param currentColorId The current color ID.
     */
    @VisibleForTesting
    public @NtpThemeColorId int maybeApplyDailyRefreshForChromeColor(
            @NtpThemeColorId int currentColorId) {
        if (!isDailyRefreshEnabled(NtpBackgroundType.CHROME_COLOR)) {
            return currentColorId;
        }

        return applyDailyRefreshForChromeColorTheme(currentColorId);
    }

    /**
     * Checks if daily refresh for a theme collection should be applied and updates the status if
     * needed.
     */
    @VisibleForTesting
    public void maybeApplyDailyRefreshForThemeCollection() {
        if (isDailyRefreshEnabled(THEME_COLLECTION)) {
            setDailyUpdateStatusForThemeCollection(TimeUtils.currentTimeMillis());
        }
    }

    @VisibleForTesting
    boolean isDailyRefreshEnabled(@NtpBackgroundType int backgroundType) {
        if (backgroundType != NtpBackgroundType.CHROME_COLOR
                && backgroundType != THEME_COLLECTION) {
            return false;
        }

        if (backgroundType == NtpBackgroundType.CHROME_COLOR
                && !NtpCustomizationUtils
                        .getIsChromeColorDailyRefreshEnabledFromSharedPreference()) {
            return false;
        }

        if (backgroundType == THEME_COLLECTION) {
            CustomBackgroundInfo customBackgroundInfo =
                    NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference();
            if (customBackgroundInfo == null || !customBackgroundInfo.isDailyRefreshEnabled) {
                return false;
            }
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
    int applyDailyRefreshForChromeColorTheme(@NtpThemeColorId int currentColorId) {
        @NtpThemeColorId int newColorId = getNextColorId(currentColorId);
        setDailyUpdateStatusForChromeColor(newColorId, TimeUtils.currentTimeMillis());
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
    void setDailyUpdateStatusForChromeColor(@NtpThemeColorId int newColorId, long timestamp) {
        mNtpThemeColorId = newColorId;
        mLastDailyUpdateTimestamp = timestamp;
        mIsDailyUpdateApplied = true;
    }

    /**
     * Sets the internal state to indicate that a daily refresh for a Theme Collection has been
     * applied.
     *
     * @param timestamp The timestamp of the refresh.
     */
    @VisibleForTesting
    void setDailyUpdateStatusForThemeCollection(long timestamp) {
        mLastDailyUpdateTimestamp = timestamp;
        mIsDailyUpdateApplied = true;
        mNtpThemeColorId = null;
    }

    /** Saves the existing daily refresh settings to the SharedPreference and resets. */
    public void maybeSaveDailyRefreshAndReset(Runnable onDailyRefreshThemeCollectionApplied) {
        if (!mIsDailyUpdateApplied || !NtpCustomizationUtils.isNtpThemeCustomizationEnabled()) {
            return;
        }

        if (mNtpThemeColorId != null) {
            NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(mNtpThemeColorId);
            mNtpThemeColorId = null;
        }

        if (NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference() == THEME_COLLECTION) {
            NtpCustomizationUtils.commitThemeCollectionDailyRefresh();
            onDailyRefreshThemeCollectionApplied.run();
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
