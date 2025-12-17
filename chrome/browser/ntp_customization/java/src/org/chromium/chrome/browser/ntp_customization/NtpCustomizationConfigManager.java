// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.daily_refresh.NtpThemeDailyRefreshManager;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.concurrent.Executor;

/** Manages the NTP's background configuration and notifies listeners of changes. */
@NullMarked
public class NtpCustomizationConfigManager {
    private static final Executor EXECUTOR =
            (Runnable r) -> PostTask.postTask(TaskTraits.USER_BLOCKING_MAY_BLOCK, r);

    private boolean mIsInitialized;
    private @NtpBackgroundImageType int mBackgroundImageType;
    // The theme collection info that the user has currently chosen.
    private @Nullable CustomBackgroundInfo mCustomBackgroundInfo;
    private @Nullable Bitmap mOriginalBitmap;
    private @Nullable BackgroundImageInfo mBackgroundImageInfo;
    private @Nullable NtpThemeColorInfo mNtpThemeColorInfo;
    private @Nullable Bitmap mDefaultSearchEngineLogoImage;
    private boolean mIsMvtToggleOn;

    /** An interface to get NewTabPage's configuration updates. */
    public interface HomepageStateListener {
        /** Called when the state of the toggle for the Most Visited Tiles section changes. */
        default void onMvtToggleChanged() {}

        /**
         * Called when a customized homepage background image is chosen.
         *
         * @param originalBitmap The new background image drawable.
         * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait
         *     and landscape matrices.
         * @param fromInitialization Whether the update of the background comes from the
         *     initialization of the {@link NtpCustomizationConfigManager}, i.e, loading the image
         *     from the device.
         * @param oldType The previously set background type for NTPs.
         * @param newType The new background type of NTPs.
         */
        default void onBackgroundImageChanged(
                Bitmap originalBitmap,
                @Nullable BackgroundImageInfo backgroundImageInfo,
                boolean fromInitialization,
                @NtpBackgroundImageType int oldType,
                @NtpBackgroundImageType int newType) {}

        /**
         * Called when the user chooses a customized homepage background color or resets to the
         * default Chrome's color.
         *
         * @param ntpThemeColorInfo The NtpThemeColorInfo for color theme.
         * @param backgroundColor The new background color.
         * @param fromInitialization Whether the update of the background comes from the
         *     initialization of the {@link NtpCustomizationConfigManager}, i.e, loading the image
         *     from the device.
         * @param oldType The previously set background type for NTPs.
         * @param newType The new background type of NTPs.
         */
        default void onBackgroundColorChanged(
                NtpThemeColorInfo ntpThemeColorInfo,
                @ColorInt int backgroundColor,
                boolean fromInitialization,
                @NtpBackgroundImageType int oldType,
                @NtpBackgroundImageType int newType) {}

        /**
         * Called when the user resets the NTP's background to default.
         *
         * @param oldType The previously set background type for NTPs.
         */
        default void onBackgroundReset(@NtpBackgroundImageType int oldType) {}

        /**
         * Called to notify observers to get refreshed system's window insets.
         *
         * @param consumeTopInset Whether the observer should consume the new window insets.
         */
        default void refreshWindowInsets(boolean consumeTopInset) {}
    }

    private static @Nullable NtpCustomizationConfigManager sInstanceForTesting;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final NtpCustomizationConfigManager sInstance = new NtpCustomizationConfigManager();
    }

    private final ObserverList<HomepageStateListener> mHomepageStateListeners;

    /** Returns the singleton instance of NtpCustomizationConfigManager. */
    public static NtpCustomizationConfigManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return NtpCustomizationConfigManager.LazyHolder.sInstance;
    }

    @VisibleForTesting
    public NtpCustomizationConfigManager() {
        mHomepageStateListeners = new ObserverList<>();

        mBackgroundImageType = NtpCustomizationUtils.getNtpBackgroundImageType();
        if (mBackgroundImageType == NtpBackgroundImageType.IMAGE_FROM_DISK) {
            mIsInitialized = true;
            BackgroundImageInfo imageInfo = NtpCustomizationUtils.readNtpBackgroundImageInfo();
            NtpCustomizationUtils.readNtpBackgroundImage(
                    (bitmap) -> {
                        onBackgroundImageAvailable(bitmap, imageInfo);
                    },
                    EXECUTOR);
        } else if (mBackgroundImageType == NtpBackgroundImageType.THEME_COLLECTION) {
            mIsInitialized = true;
            NtpThemeDailyRefreshManager ntpThemeDailyRefreshManager =
                    NtpThemeDailyRefreshManager.getInstance();
            BackgroundImageInfo imageInfo =
                    ntpThemeDailyRefreshManager.getNtpBackgroundImageInfoForThemeCollection();
            ntpThemeDailyRefreshManager.readNtpBackgroundImageForThemeCollection(
                    (bitmap) -> {
                        onBackgroundImageAvailable(bitmap, imageInfo);
                    },
                    EXECUTOR);
            mCustomBackgroundInfo =
                    ntpThemeDailyRefreshManager.getNtpCustomBackgroundInfoForThemeCollection();
        }

        mIsMvtToggleOn =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_MVT_VISIBLE, true);
    }

    @VisibleForTesting
    void onBackgroundImageAvailable(
            @Nullable Bitmap bitmap, @Nullable BackgroundImageInfo imageInfo) {
        if (bitmap == null) {
            // TODO(crbug.com/423579377): need to update the trailing icons in the NTP appearance
            // bottom sheet.
            if (mBackgroundImageType != NtpBackgroundImageType.DEFAULT) {
                onBackgroundReset();
            }
            return;
        }
        onBackgroundChangedImpl(
                bitmap, imageInfo, NtpBackgroundImageType.DEFAULT, /* fromInitialization= */ true);
    }

    @VisibleForTesting
    void maybeInitializeColorTheme(Context context) {
        if (mIsInitialized) return;

        mIsInitialized = true;
        if (mBackgroundImageType == NtpBackgroundImageType.CHROME_COLOR) {
            @NtpThemeColorId
            int colorId =
                    NtpThemeDailyRefreshManager.getInstance()
                            .getNtpThemeColorIdForChromeColorTheme();
            mNtpThemeColorInfo = NtpThemeColorUtils.createNtpThemeColorInfo(context, colorId);
            notifyBackgroundColorChanged(
                    context,
                    /* fromInitialization= */ true,
                    /* oldType= */ NtpBackgroundImageType.DEFAULT);

        } else if (mBackgroundImageType == NtpBackgroundImageType.COLOR_FROM_HEX) {
            @ColorInt
            int backgroundColor =
                    NtpCustomizationUtils.getBackgroundColorFromSharedPreference(
                            NtpThemeColorUtils.getDefaultBackgroundColor(context));
            @ColorInt
            int primaryColor =
                    NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference();
            mNtpThemeColorInfo =
                    new NtpThemeColorFromHexInfo(context, backgroundColor, primaryColor);
            notifyBackgroundColorChanged(
                    context, /* fromInitialization= */ true, NtpBackgroundImageType.DEFAULT);
        }
    }

    /**
     * Adds a {@link HomepageStateListener} to receive updates when the home modules state changes.
     *
     * @param listener The listener instance to add.
     * @param context The Application context.
     * @param skipNotify Whether to skip being notified immediately.
     */
    public void addListener(HomepageStateListener listener, Context context, boolean skipNotify) {
        mHomepageStateListeners.addObserver(listener);
        if (skipNotify) return;

        if (!mIsInitialized) {
            maybeInitializeColorTheme(context);
            return;
        }

        switch (mBackgroundImageType) {
            case IMAGE_FROM_DISK, NtpBackgroundImageType.THEME_COLLECTION -> {
                if (mOriginalBitmap != null) {
                    // It is possible that when addListener() is called, the background image hasn't
                    // been loaded, skip notifying the listener now.
                    listener.onBackgroundImageChanged(
                            mOriginalBitmap,
                            mBackgroundImageInfo,
                            /* fromInitialization= */ true,
                            NtpBackgroundImageType.DEFAULT,
                            mBackgroundImageType);
                }
            }
            case NtpBackgroundImageType.CHROME_COLOR, NtpBackgroundImageType.COLOR_FROM_HEX ->
                    listener.onBackgroundColorChanged(
                            assumeNonNull(mNtpThemeColorInfo),
                            getBackgroundColor(context),
                            /* fromInitialization= */ true,
                            NtpBackgroundImageType.DEFAULT,
                            mBackgroundImageType);

            case NtpBackgroundImageType.DEFAULT -> listener.onBackgroundReset(mBackgroundImageType);
        }
    }

    /**
     * Removes the given listener from the state listener list.
     *
     * @param listener The listener to remove.
     */
    public void removeListener(HomepageStateListener listener) {
        mHomepageStateListeners.removeObserver(listener);
    }

    /**
     * Called when a user uploaded image is selected.
     *
     * @param bitmap The new background image bitmap before transformations.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     */
    public void onUploadedImageSelected(Bitmap bitmap, BackgroundImageInfo backgroundImageInfo) {
        @NtpBackgroundImageType int oldType = mBackgroundImageType;

        mBackgroundImageType = IMAGE_FROM_DISK;

        NtpCustomizationUtils.saveBackgroundInfo(
                /* customBackgroundInfo= */ null,
                bitmap,
                backgroundImageInfo,
                /* skipSavingPrimaryColor= */ false);

        onBackgroundChanged(bitmap, backgroundImageInfo, oldType);
    }

    /**
     * Called when a Chrome theme collection image is selected.
     *
     * @param bitmap The new background image bitmap before transformations.
     * @param customBackgroundInfo The {@link CustomBackgroundInfo} object containing the theme
     *     collection info.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     */
    public void onThemeCollectionImageSelected(
            Bitmap bitmap,
            CustomBackgroundInfo customBackgroundInfo,
            BackgroundImageInfo backgroundImageInfo) {
        @NtpBackgroundImageType int oldType = mBackgroundImageType;
        mBackgroundImageType = NtpBackgroundImageType.THEME_COLLECTION;
        mCustomBackgroundInfo = customBackgroundInfo;
        onBackgroundChanged(bitmap, backgroundImageInfo, oldType);
        // Updates the daily refresh timestamp if daily refresh enabled.
        NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                TimeUtils.currentTimeMillis(), mBackgroundImageType, mCustomBackgroundInfo);
    }

    /**
     * Notifies listeners about the NTP's background change.
     *
     * @param bitmap The new background image bitmap before transformations.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     * @param oldBackgroundImageType The previous type of the NTP's background image.
     */
    public void onBackgroundChanged(
            Bitmap bitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @NtpBackgroundImageType int oldBackgroundImageType) {
        onBackgroundChangedImpl(
                bitmap,
                backgroundImageInfo,
                oldBackgroundImageType,
                /* fromInitialization= */ false);
    }

    private void onBackgroundChangedImpl(
            Bitmap bitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @NtpBackgroundImageType int oldBackgroundImageType,
            boolean fromInitialization) {
        mOriginalBitmap = bitmap;
        mBackgroundImageInfo = backgroundImageInfo;
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(mBackgroundImageType);
        NtpCustomizationUtils.resetCustomizedColors();

        notifyBackgroundImageChanged(
                bitmap, backgroundImageInfo, fromInitialization, oldBackgroundImageType);
    }

    /**
     * Notifies listeners about the NTP's background color change: 1) If a new customized color is
     * chosen: save the selected background color to the SharedPreference. 2) If resting to Chrome's
     * default color: delete the color key from the SharedPreference.
     *
     * @param context : The current Activity context.
     * @param colorInfo : The new NTP's background color.
     * @param backgroundImageType : The new background image type.
     */
    public void onBackgroundColorChanged(
            Context context,
            NtpThemeColorInfo colorInfo,
            @NtpBackgroundImageType int backgroundImageType) {
        assert backgroundImageType == NtpBackgroundImageType.CHROME_COLOR
                || backgroundImageType == NtpBackgroundImageType.COLOR_FROM_HEX;

        @NtpBackgroundImageType int oldType = mBackgroundImageType;
        mBackgroundImageType = backgroundImageType;
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(mBackgroundImageType);
        mNtpThemeColorInfo = colorInfo;

        cleanupBackgroundImage();
        notifyBackgroundColorChanged(context, /* fromInitialization= */ false, oldType);

        if (mBackgroundImageType == NtpBackgroundImageType.CHROME_COLOR) {
            NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(assumeNonNull(colorInfo).id);
            // Updates the daily refresh timestamp if enabled.
            NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                    TimeUtils.currentTimeMillis(),
                    mBackgroundImageType,
                    /* customBackgroundInfo= */ null);
        }

        if (colorInfo instanceof NtpThemeColorFromHexInfo colorFromHexInfo) {
            NtpCustomizationUtils.setBackgroundColorToSharedPreference(
                    colorFromHexInfo.backgroundColor);
            NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(
                    colorFromHexInfo.primaryColor);
        }
    }

    /** Notifies listeners about the NTP's customized background is reset. */
    public void onBackgroundReset() {
        @NtpBackgroundImageType int oldType = mBackgroundImageType;
        mBackgroundImageType = NtpBackgroundImageType.DEFAULT;

        cleanupOnBackgroundTypeChanged(oldType);
        NtpCustomizationUtils.removeNtpBackgroundImageTypeFromSharedPreference();
        notifyBackgroundReset(oldType);
    }

    /**
     * Notifies the NTP's background image is changed.
     *
     * @param originalBitmap The new background image bitmap before transformations.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     * @param fromInitialization Whether the update of the background comes from the initialization
     *     of the {@link NtpCustomizationConfigManager}, i.e, loading the image from the device.
     */
    @VisibleForTesting
    public void notifyBackgroundImageChanged(
            Bitmap originalBitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            boolean fromInitialization,
            @NtpBackgroundImageType int oldType) {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundImageChanged(
                    originalBitmap,
                    backgroundImageInfo,
                    fromInitialization,
                    oldType,
                    mBackgroundImageType);
        }
    }

    /**
     * Notifies the NTP's background color is changed.
     *
     * @param context Used to get a color based on the theme.
     * @param fromInitialization Whether the update of the background comes from the initialization
     *     of the {@link NtpCustomizationConfigManager}, i.e,loading the image from the device.
     * @param oldType The previously set background type for NTP.
     */
    @VisibleForTesting
    public void notifyBackgroundColorChanged(
            Context context, boolean fromInitialization, @NtpBackgroundImageType int oldType) {
        @ColorInt
        int backgroundColor =
                NtpThemeColorUtils.getBackgroundColorFromColorInfo(context, mNtpThemeColorInfo);
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundColorChanged(
                    assumeNonNull(mNtpThemeColorInfo),
                    backgroundColor,
                    fromInitialization,
                    oldType,
                    mBackgroundImageType);
        }
    }

    /**
     * Notifies the NTP's background color is changed.
     *
     * @param oldType The previously set background type for NTP.
     */
    @VisibleForTesting
    public void notifyBackgroundReset(@NtpBackgroundImageType int oldType) {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundReset(oldType);
        }
    }

    /** Notifies observers to refresh the system's WindowInsets. */
    public void notifyRefreshWindowInsets(boolean consumeTopInset) {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.refreshWindowInsets(consumeTopInset);
        }
    }

    /** Returns the user's preference for whether the Most Visited Tiles section is visible. */
    public boolean getPrefIsMvtToggleOn() {
        return mIsMvtToggleOn;
    }

    /**
     * Sets the user preference for whether the Most Visited Tiles section is visible.
     *
     * @param isMvtToggleOn True to show the section, false to hide it.
     */
    public void setPrefIsMvtToggleOn(boolean isMvtToggleOn) {
        mIsMvtToggleOn = isMvtToggleOn;
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_MVT_VISIBLE, isMvtToggleOn);

        // Notifies all the listeners.
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onMvtToggleChanged();
        }
    }

    /** Gets the NTP's background image type. */
    public @NtpBackgroundImageType int getBackgroundImageType() {
        return mBackgroundImageType;
    }

    /**
     * Returns the current background color for NTP. Needs to use the Activity's context rather than
     * the application's context, which isn't themed and will provide a wrong color.
     *
     * @param context The current Activity context. It is themed and can provide the correct color.
     */
    public @ColorInt int getBackgroundColor(Context context) {
        if (!mIsInitialized || mNtpThemeColorInfo == null) {
            return NtpThemeColorUtils.getDefaultBackgroundColor(context);
        }

        return NtpThemeColorUtils.getBackgroundColorFromColorInfo(context, mNtpThemeColorInfo);
    }

    public @Nullable CustomBackgroundInfo getCustomBackgroundInfo() {
        return mCustomBackgroundInfo;
    }

    public @Nullable NtpThemeColorInfo getNtpThemeColorInfo() {
        return mNtpThemeColorInfo;
    }

    public void setDefaultSearchEngineLogoBitmap(@Nullable Bitmap logoBitmap) {
        mDefaultSearchEngineLogoImage = logoBitmap;
    }

    public @Nullable Bitmap getDefaultSearchEngineLogoBitmap() {
        return mDefaultSearchEngineLogoImage;
    }

    /**
     * Sets a NtpCustomizationConfigManager instance for testing.
     *
     * @param instance The instance to set.
     */
    public static void setInstanceForTesting(@Nullable NtpCustomizationConfigManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public void setNtpThemeColorInfoForTesting(@Nullable NtpThemeColorInfo colorInfo) {
        mNtpThemeColorInfo = colorInfo;
    }

    public @Nullable BackgroundImageInfo getBackgroundImageInfoForTesting() {
        return mBackgroundImageInfo;
    }

    public int getListenersSizeForTesting() {
        return mHomepageStateListeners.size();
    }

    public void setBackgroundImageTypeForTesting(@NtpBackgroundImageType int backgroundImageType) {
        mBackgroundImageType = backgroundImageType;
    }

    void setIsInitializedForTesting(boolean isInitialized) {
        mIsInitialized = isInitialized;
    }

    /** Cleans up background bitmap image related info. */
    private void cleanupBackgroundImage() {
        mBackgroundImageInfo = null;
        mOriginalBitmap = null;
    }

    private void cleanupOnBackgroundTypeChanged(@NtpBackgroundImageType int oldType) {
        if (oldType == mBackgroundImageType) return;

        switch (oldType) {
            case CHROME_COLOR -> NtpCustomizationUtils.resetCustomizedColors();
            case IMAGE_FROM_DISK, THEME_COLLECTION -> cleanupBackgroundImage();
        }
    }

    public void resetForTesting() {
        mHomepageStateListeners.clear();
        mIsInitialized = false;
        mBackgroundImageType = NtpBackgroundImageType.DEFAULT;
        cleanupBackgroundImage();
        mIsMvtToggleOn = false;
    }

    void setCustomBackgroundInfoForTesting(CustomBackgroundInfo customBackgroundInfo) {
        mCustomBackgroundInfo = customBackgroundInfo;
    }

    @Nullable Bitmap getOriginalBitmapForTesting() {
        return mOriginalBitmap;
    }
}
