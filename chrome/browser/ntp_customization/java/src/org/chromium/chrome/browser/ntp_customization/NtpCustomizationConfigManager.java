// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeStateProvider;
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
    private @NtpBackgroundType int mBackgroundType;
    // The theme collection info that the user has currently chosen.
    private @Nullable CustomBackgroundInfo mCustomBackgroundInfo;
    private @Nullable Bitmap mOriginalBitmap;
    private @Nullable BackgroundImageInfo mBackgroundImageInfo;
    private @Nullable NtpThemeColorInfo mNtpThemeColorInfo;
    private @Nullable Bitmap mDefaultSearchEngineLogoImage;
    private @Nullable NtpThemeStateProvider mNtpThemeStateProvider;
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
                BackgroundImageInfo backgroundImageInfo,
                boolean fromInitialization,
                @NtpBackgroundType int oldType,
                @NtpBackgroundType int newType) {}

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
                @NtpBackgroundType int oldType,
                @NtpBackgroundType int newType) {}

        /**
         * Called when the user resets the NTP's background to default.
         *
         * @param oldType The previously set background type for NTPs.
         */
        default void onBackgroundReset(@NtpBackgroundType int oldType) {}
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

        mBackgroundType = NtpCustomizationUtils.getNtpBackgroundType();
        if (mBackgroundType == NtpBackgroundType.IMAGE_FROM_DISK) {
            mIsInitialized = true;
            BackgroundImageInfo imageInfo = NtpCustomizationUtils.readNtpBackgroundImageInfo();
            NtpCustomizationUtils.readNtpBackgroundImage(
                    (bitmap) -> {
                        onBackgroundImageAvailable(bitmap, imageInfo);
                    },
                    EXECUTOR);
        } else if (mBackgroundType == NtpBackgroundType.THEME_COLLECTION) {
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
            if (mBackgroundType != NtpBackgroundType.DEFAULT) {
                onBackgroundReset();
            }
            return;
        }
        onBackgroundImageChangedImpl(
                bitmap, imageInfo, NtpBackgroundType.DEFAULT, /* fromInitialization= */ true);
    }

    @VisibleForTesting
    void maybeInitializeColorTheme(Context context) {
        if (mIsInitialized) return;

        mIsInitialized = true;
        if (mBackgroundType == NtpBackgroundType.CHROME_COLOR) {
            @NtpThemeColorId
            int colorId =
                    NtpThemeDailyRefreshManager.getInstance()
                            .getNtpThemeColorIdForChromeColorTheme();
            mNtpThemeColorInfo = NtpThemeColorUtils.createNtpThemeColorInfo(context, colorId);
            notifyBackgroundColorChanged(
                    context,
                    /* fromInitialization= */ true,
                    /* oldType= */ NtpBackgroundType.DEFAULT);

        } else if (mBackgroundType == NtpBackgroundType.COLOR_FROM_HEX) {
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
                    context, /* fromInitialization= */ true, NtpBackgroundType.DEFAULT);
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

        switch (mBackgroundType) {
            case IMAGE_FROM_DISK, NtpBackgroundType.THEME_COLLECTION -> {
                if (mOriginalBitmap != null) {
                    // It is possible that when addListener() is called, the background image hasn't
                    // been loaded, skip notifying the listener now.
                    BackgroundImageInfo backgroundImageInfo = mBackgroundImageInfo;
                    if (backgroundImageInfo == null) {
                        backgroundImageInfo =
                                NtpCustomizationUtils.getDefaultBackgroundImageInfo(
                                        ContextUtils.getApplicationContext(), mOriginalBitmap);
                    }
                    listener.onBackgroundImageChanged(
                            mOriginalBitmap,
                            backgroundImageInfo,
                            /* fromInitialization= */ true,
                            NtpBackgroundType.DEFAULT,
                            mBackgroundType);
                }
            }
            case NtpBackgroundType.CHROME_COLOR, NtpBackgroundType.COLOR_FROM_HEX ->
                    listener.onBackgroundColorChanged(
                            assumeNonNull(mNtpThemeColorInfo),
                            getBackgroundColor(context),
                            /* fromInitialization= */ true,
                            NtpBackgroundType.DEFAULT,
                            mBackgroundType);

            case NtpBackgroundType.DEFAULT -> listener.onBackgroundReset(mBackgroundType);
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
        @NtpBackgroundType int oldType = mBackgroundType;

        mBackgroundType = IMAGE_FROM_DISK;

        NtpCustomizationUtils.saveBackgroundInfo(
                /* customBackgroundInfo= */ null,
                bitmap,
                backgroundImageInfo,
                /* skipSavingPrimaryColor= */ false);

        onBackgroundImageChanged(bitmap, backgroundImageInfo, oldType);
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
        @NtpBackgroundType int oldType = mBackgroundType;
        mBackgroundType = NtpBackgroundType.THEME_COLLECTION;
        mCustomBackgroundInfo = customBackgroundInfo;
        onBackgroundImageChanged(bitmap, backgroundImageInfo, oldType);
        // Updates the daily refresh timestamp if daily refresh enabled.
        NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                TimeUtils.currentTimeMillis(), mBackgroundType, mCustomBackgroundInfo);
    }

    /**
     * Notifies listeners about the NTP's background image change.
     *
     * @param bitmap The new background image bitmap before transformations.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     * @param oldBackgroundType The previous type of the NTP's background.
     */
    public void onBackgroundImageChanged(
            Bitmap bitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @NtpBackgroundType int oldBackgroundType) {
        onBackgroundImageChangedImpl(
                bitmap, backgroundImageInfo, oldBackgroundType, /* fromInitialization= */ false);
    }

    private void onBackgroundImageChangedImpl(
            Bitmap bitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @NtpBackgroundType int oldBackgroundType,
            boolean fromInitialization) {
        mOriginalBitmap = bitmap;
        mBackgroundImageInfo =
                backgroundImageInfo == null
                        ? NtpCustomizationUtils.getDefaultBackgroundImageInfo(
                                ContextUtils.getApplicationContext(), bitmap)
                        : backgroundImageInfo;
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(mBackgroundType);
        cleanupChromeColors();

        notifyBackgroundImageChanged(
                bitmap, mBackgroundImageInfo, fromInitialization, oldBackgroundType);
    }

    /**
     * Notifies listeners about the NTP's background color change: 1) If a new customized color is
     * chosen: save the selected background color to the SharedPreference. 2) If resting to Chrome's
     * default color: delete the color key from the SharedPreference.
     *
     * @param context : The current Activity context.
     * @param colorInfo : The new NTP's background color.
     * @param backgroundType : The new background image type.
     */
    public void onBackgroundColorChanged(
            Context context, NtpThemeColorInfo colorInfo, @NtpBackgroundType int backgroundType) {
        assert backgroundType == NtpBackgroundType.CHROME_COLOR
                || backgroundType == NtpBackgroundType.COLOR_FROM_HEX;

        @NtpBackgroundType int oldType = mBackgroundType;
        mBackgroundType = backgroundType;
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(mBackgroundType);
        mNtpThemeColorInfo = colorInfo;

        cleanupBackgroundImage();
        notifyBackgroundColorChanged(context, /* fromInitialization= */ false, oldType);

        if (mBackgroundType == NtpBackgroundType.CHROME_COLOR) {
            NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(assumeNonNull(colorInfo).id);
            // Updates the daily refresh timestamp if enabled.
            NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                    TimeUtils.currentTimeMillis(),
                    mBackgroundType,
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
        @NtpBackgroundType int oldType = mBackgroundType;
        mBackgroundType = NtpBackgroundType.DEFAULT;

        cleanupOnBackgroundTypeChanged(oldType);
        NtpCustomizationUtils.removeNtpBackgroundTypeFromSharedPreference();
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
            BackgroundImageInfo backgroundImageInfo,
            boolean fromInitialization,
            @NtpBackgroundType int oldType) {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundImageChanged(
                    originalBitmap,
                    backgroundImageInfo,
                    fromInitialization,
                    oldType,
                    mBackgroundType);
        }

        notifyNtpThemeStateProvider();
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
            Context context, boolean fromInitialization, @NtpBackgroundType int oldType) {
        @ColorInt
        int backgroundColor =
                NtpThemeColorUtils.getBackgroundColorFromColorInfo(context, mNtpThemeColorInfo);
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundColorChanged(
                    assumeNonNull(mNtpThemeColorInfo),
                    backgroundColor,
                    fromInitialization,
                    oldType,
                    mBackgroundType);
        }

        notifyNtpThemeStateProvider();
    }

    /**
     * Notifies the NTP's background color is changed.
     *
     * @param oldType The previously set background type for NTP.
     */
    @VisibleForTesting
    public void notifyBackgroundReset(@NtpBackgroundType int oldType) {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundReset(oldType);
        }

        notifyNtpThemeStateProvider();
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
    public @NtpBackgroundType int getBackgroundType() {
        return mBackgroundType;
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

    public void setBackgroundTypeForTesting(@NtpBackgroundType int backgroundType) {
        mBackgroundType = backgroundType;
    }

    void setIsInitializedForTesting(boolean isInitialized) {
        mIsInitialized = isInitialized;
    }

    /** Cleans up background bitmap image related info. */
    private void cleanupBackgroundImage() {
        mBackgroundImageInfo = null;
        mOriginalBitmap = null;
        NtpCustomizationUtils.resetCustomizedImage();
    }

    private void cleanupChromeColors() {
        mNtpThemeColorInfo = null;
        NtpCustomizationUtils.resetCustomizedColors();
    }

    private void cleanupOnBackgroundTypeChanged(@NtpBackgroundType int oldType) {
        if (oldType == mBackgroundType) return;

        switch (oldType) {
            case CHROME_COLOR -> cleanupChromeColors();
            case IMAGE_FROM_DISK, THEME_COLLECTION -> cleanupBackgroundImage();
        }
    }

    /**
     * Notifies the NtpThemeStateProvider when the NTP's customize background is changed. This
     * should be called after notifying NTPs.
     */
    private void notifyNtpThemeStateProvider() {
        // Notifies NtpThemeStateProvider last to ensure the NTP Tab has already processed its
        // update. The Tab must first recompute its light icon tint state, as this state is
        // subsequently queried by AdjustedTopUiThemeColorProvider.
        if (mNtpThemeStateProvider == null) {
            mNtpThemeStateProvider = NtpThemeStateProvider.getInstance();
        }
        mNtpThemeStateProvider.notifyCustomBackgroundChanged();
    }

    public void resetForTesting() {
        mHomepageStateListeners.clear();
        mIsInitialized = false;
        mBackgroundType = NtpBackgroundType.DEFAULT;
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
