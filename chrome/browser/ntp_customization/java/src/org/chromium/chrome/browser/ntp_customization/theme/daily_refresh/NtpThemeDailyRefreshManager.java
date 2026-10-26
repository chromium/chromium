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
    // Whether a due daily refresh of a theme collection should be skipped because the next image
    // hasn't been completely prefetched, e.g., the fetch failed or the process was killed before it
    // completed.
    private boolean mShouldSkipDailyRefreshForThemeCollection;
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
    public @Nullable @ColorInt Integer getNtpThemeColorForThemeCollection() {
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
            Callback<@Nullable Bitmap> callback, Executor executor, @Nullable String filePath) {
        if (mIsDailyUpdateApplied) {
            NtpCustomizationUtils.readDailyRefreshNtpBackgroundImage(callback, executor);
            return;
        }

        NtpCustomizationUtils.readNtpBackgroundImage(callback, executor, filePath);
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
     * needed. The refresh is only applied if the next image has been prefetched. The daily refresh
     * info is only saved after the image file has been completely written. Otherwise, it is skipped
     * and the next image is fetched again in {@link #maybeSaveDailyRefreshAndReset}.
     */
    @VisibleForTesting
    public void maybeApplyDailyRefreshForThemeCollection() {
        if (!isDailyRefreshEnabled(THEME_COLLECTION)) {
            return;
        }

        // The refresh is skipped if the next image hasn't been prefetched, e.g., the fetch failed,
        // or the process was killed or the write failed before the image file was completely
        // written, since the daily refresh info is only saved after the image file. The current
        // image is kept. The timestamp is still updated so that the daily refresh isn't applied
        // later in this process, after the activity theme and the NTP background have been loaded
        // from the current image.
        boolean isPrefetched =
                NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference()
                        != null;
        setDailyUpdateStatusForThemeCollection(TimeUtils.currentTimeMillis(), isPrefetched);
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
     * Sets the internal state to indicate that a due daily refresh for a Theme Collection has been
     * applied or skipped.
     *
     * @param timestamp The timestamp of the refresh.
     * @param shouldApply Whether the refresh should be applied, i.e., the next image has been
     *     prefetched. If so, the getters return the prefetched daily refresh data until it's
     *     committed in {@link #maybeSaveDailyRefreshAndReset}. Otherwise, the refresh is skipped.
     */
    @VisibleForTesting
    void setDailyUpdateStatusForThemeCollection(long timestamp, boolean shouldApply) {
        mLastDailyUpdateTimestamp = timestamp;
        mIsDailyUpdateApplied = shouldApply;
        mShouldSkipDailyRefreshForThemeCollection = !shouldApply;
        mNtpThemeColorId = null;
    }

    /**
     * Saves the existing daily refresh settings to the SharedPreference and resets.
     *
     * @param onDailyRefreshThemeCollectionApplied Run if the background is a theme collection and a
     *     daily refresh was applied or skipped, to fetch the image for the next daily refresh.
     */
    public void maybeSaveDailyRefreshAndReset(Runnable onDailyRefreshThemeCollectionApplied) {
        if ((!mIsDailyUpdateApplied && !mShouldSkipDailyRefreshForThemeCollection)
                || !NtpCustomizationUtils.isNtpThemeCustomizationEnabled()) {
            return;
        }

        if (mNtpThemeColorId != null) {
            NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(mNtpThemeColorId);
        }

        if (NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference() == THEME_COLLECTION) {
            // A skipped daily refresh has no prefetched image to commit. A stale daily refresh
            // image file may exist, which mustn't replace the current image.
            if (mIsDailyUpdateApplied) {
                NtpCustomizationUtils.commitThemeCollectionDailyRefresh();
            }
            onDailyRefreshThemeCollectionApplied.run();
        }

        if (mLastDailyUpdateTimestamp != null) {
            NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(
                    mLastDailyUpdateTimestamp);
        }
        discardPendingDailyRefresh();
    }

    /**
     * Discards a daily refresh that was applied in memory but not saved to SharedPreferences yet.
     * Called when the background in SharedPreferences is replaced, e.g. by a synced theme or a
     * local selection, so that the getters stop returning the stale rotation and {@link
     * #maybeSaveDailyRefreshAndReset} doesn't save it over the new background.
     */
    public void discardPendingDailyRefresh() {
        mIsDailyUpdateApplied = false;
        mShouldSkipDailyRefreshForThemeCollection = false;
        mLastDailyUpdateTimestamp = null;
        mNtpThemeColorId = null;
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

    public boolean getShouldSkipDailyRefreshForThemeCollectionForTesting() {
        return mShouldSkipDailyRefreshForThemeCollection;
    }

    public @Nullable Long getLastDailyUpdateTimestampForTesting() {
        return mLastDailyUpdateTimestamp;
    }

    public @Nullable @NtpThemeColorId Integer getNtpThemeColorIdForTesting() {
        return mNtpThemeColorId;
    }
}
