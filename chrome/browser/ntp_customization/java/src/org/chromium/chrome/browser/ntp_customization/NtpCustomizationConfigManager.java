// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.app.Activity;
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
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataCustomizedColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataImageBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataUploadImage;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.concurrent.Executor;

/** Manages the NTP's background configuration and notifies listeners of changes. */
@NullMarked
public class NtpCustomizationConfigManager {
    public static final Executor EXECUTOR =
            (Runnable r) -> PostTask.postTask(TaskTraits.USER_BLOCKING_MAY_BLOCK, r);

    private final boolean mIsNtpCustomizationSyncEnabled;
    private boolean mIsInitialized;
    private @NtpBackgroundType int mBackgroundType;
    // The theme collection info that the user has currently chosen.
    private @Nullable CustomBackgroundInfo mCustomBackgroundInfo;
    private @Nullable Bitmap mOriginalBitmap;
    private @Nullable BackgroundImageInfo mBackgroundImageInfo;
    private @Nullable NtpBackgroundDataBase mNtpBackgroundData;
    private @Nullable Bitmap mDefaultSearchEngineLogoImage;
    private @Nullable NtpThemeStateProvider mNtpThemeStateProvider;
    private @Nullable NtpBackgroundDataManager mNtpBackgroundDataManager;
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
        mIsNtpCustomizationSyncEnabled = NtpCustomizationUtils.isNTPCustomizationSyncEnabled();
        String filePath = NtpCustomizationUtils.getBackgroundImageFilePathFromSharedPreference();
        String fileIdHash = NtpCustomizationUtils.getFileIdHashFromFilePath(filePath);

        if (mBackgroundType == NtpBackgroundType.IMAGE_FROM_DISK) {
            mIsInitialized = true;
            BackgroundImageInfo imageInfo = NtpCustomizationUtils.readNtpBackgroundImageInfo();
            @ColorInt
            int primaryColor =
                    NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference();
            mNtpBackgroundData =
                    new NtpBackgroundDataUploadImage(
                            PlatformType.ANDROID,
                            imageInfo,
                            /* bitmap= */ null,
                            primaryColor,
                            fileIdHash);
            NtpCustomizationUtils.readNtpBackgroundImage(
                    (bitmap) -> {
                        onBackgroundImageLoadedFromDisk(bitmap, imageInfo);
                    },
                    EXECUTOR,
                    filePath);
        } else if (mBackgroundType == NtpBackgroundType.THEME_COLLECTION) {
            mIsInitialized = true;
            NtpThemeDailyRefreshManager ntpThemeDailyRefreshManager =
                    NtpThemeDailyRefreshManager.getInstance();
            BackgroundImageInfo imageInfo =
                    ntpThemeDailyRefreshManager.getNtpBackgroundImageInfoForThemeCollection();
            @ColorInt
            int primaryColor = ntpThemeDailyRefreshManager.getNtpThemeColorForThemeCollection();
            ntpThemeDailyRefreshManager.readNtpBackgroundImageForThemeCollection(
                    (bitmap) -> {
                        onBackgroundImageLoadedFromDisk(bitmap, imageInfo);
                    },
                    EXECUTOR,
                    filePath);
            mCustomBackgroundInfo =
                    ntpThemeDailyRefreshManager.getNtpCustomBackgroundInfoForThemeCollection();
            mNtpBackgroundData =
                    new NtpBackgroundDataThemeCollection(
                            PlatformType.ANDROID,
                            assumeNonNull(mCustomBackgroundInfo),
                            imageInfo,
                            /* bitmap= */ null,
                            primaryColor,
                            fileIdHash);
        }

        mIsMvtToggleOn =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_MVT_VISIBLE, true);
    }

    @VisibleForTesting
    void onBackgroundImageLoadedFromDisk(
            @Nullable Bitmap bitmap, @Nullable BackgroundImageInfo imageInfo) {
        if (bitmap == null) {
            // TODO(crbug.com/423579377): need to update the trailing icons in the NTP appearance
            // bottom sheet.
            if (mBackgroundType != NtpBackgroundType.DEFAULT) {
                onBackgroundReset();
            }
            return;
        }

        if (mNtpBackgroundData != null
                && mNtpBackgroundData
                        instanceof NtpBackgroundDataImageBase ntpBackgroundDataImageBase) {
            ntpBackgroundDataImageBase.setBitmap(bitmap);
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
            mNtpBackgroundData =
                    new NtpBackgroundDataColor(
                            context,
                            PlatformType.ANDROID,
                            colorId,
                            NtpCustomizationUtils
                                    .getIsChromeColorDailyRefreshEnabledFromSharedPreference());
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
            mNtpBackgroundData =
                    new NtpBackgroundDataCustomizedColor(
                            PlatformType.ANDROID,
                            new NtpThemeColorFromHexInfo(context, backgroundColor, primaryColor));
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
            case NtpBackgroundType.CHROME_COLOR -> {
                if (mNtpBackgroundData != null
                        && mNtpBackgroundData
                                instanceof NtpBackgroundDataColor ntpBackgroundDataColor) {
                    listener.onBackgroundColorChanged(
                            ntpBackgroundDataColor.getNtpThemeColorInfo(),
                            getBackgroundColor(context),
                            /* fromInitialization= */ true,
                            NtpBackgroundType.DEFAULT,
                            mBackgroundType);
                }
            }

            case NtpBackgroundType.COLOR_FROM_HEX -> {
                if (mNtpBackgroundData != null
                        && mNtpBackgroundData
                                instanceof
                                NtpBackgroundDataCustomizedColor ntpBackgroundDataCustomizedColor) {
                    listener.onBackgroundColorChanged(
                            ntpBackgroundDataCustomizedColor.getNtpThemeColorFromHexInfo(),
                            getBackgroundColor(context),
                            /* fromInitialization= */ true,
                            NtpBackgroundType.DEFAULT,
                            mBackgroundType);
                }
            }

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
     * Called when users selected a new NTP background theme.
     *
     * @param context The application context.
     * @param backgroundData The selected NTP background theme data.
     */
    public void onBackgroundDataChanged(
            Context context, @Nullable NtpBackgroundDataBase backgroundData) {
        if (backgroundData == null) {
            onBackgroundReset();
        } else if (backgroundData instanceof NtpBackgroundDataColor ntpBackgroundDataColor) {
            if (ntpBackgroundDataColor.getThemeColorId() == NtpThemeColorId.DEFAULT) {
                onBackgroundReset();
            } else {
                onBackgroundColorChanged(context, backgroundData);
            }
        } else if (backgroundData.getBackgroundType() == NtpBackgroundType.COLOR_FROM_HEX) {
            onBackgroundColorChanged(context, backgroundData);
        } else if (backgroundData instanceof NtpBackgroundDataUploadImage uploadImageData) {
            onUploadedImageSelected(uploadImageData);
        } else if (backgroundData instanceof NtpBackgroundDataThemeCollection themeCollectionData) {
            onThemeCollectionImageSelected(themeCollectionData);
        }
    }

    /**
     * Called when a user uploaded image is selected.
     *
     * @param uploadImageData The {@link NtpBackgroundDataUploadImage} object containing the
     *     background image and info.
     */
    private void onUploadedImageSelected(NtpBackgroundDataUploadImage uploadImageData) {
        @NtpBackgroundType int oldType = mBackgroundType;

        mBackgroundType = IMAGE_FROM_DISK;
        mNtpBackgroundData = uploadImageData;
        // Saves the file path to the SharedPreference.
        NtpCustomizationUtils.setBackgroundImageFilePathToSharedPreference(
                NtpCustomizationUtils.getBackgroundImageFileFromPath(
                                uploadImageData.getLastUploadImageFilePath())
                        .getAbsolutePath());

        Bitmap bitmap = uploadImageData.getBitmap();
        if (bitmap == null) {
            //  TODO(https://crbug.com/488439751): Removes this early exit when we load the bitmap.
            return;
        }

        BackgroundImageInfo backgroundImageInfo =
                assumeNonNull(uploadImageData.getBackgroundImageInfo());

        boolean fromHistoryData = uploadImageData.getPrimaryColor() != null;
        // If this upload image data is selected from history item list, the primary color and image
        // bitmap have been saved to disk before. Thus, we don't need to pick the primary color or
        // save the bitmap again, but only update the current primary color to the Shared
        // Preference.
        @ColorInt
        Integer primaryColor =
                NtpCustomizationUtils.saveBackgroundInfo(
                        uploadImageData,
                        fromHistoryData ? null : bitmap,
                        backgroundImageInfo,
                        fromHistoryData);
        if (!fromHistoryData) {
            uploadImageData.setPrimaryColor(primaryColor);
        }

        onBackgroundImageChanged(bitmap, backgroundImageInfo, oldType);
    }

    /**
     * Called when a Chrome theme collection image is selected.
     *
     * @param themeCollectionData The {@link NtpBackgroundDataThemeCollection} object containing the
     *     theme collection info and background image info.
     */
    private void onThemeCollectionImageSelected(
            NtpBackgroundDataThemeCollection themeCollectionData) {
        @NtpBackgroundType int oldType = mBackgroundType;
        mBackgroundType = NtpBackgroundType.THEME_COLLECTION;
        mCustomBackgroundInfo = themeCollectionData.getCustomBackgroundInfo();
        mNtpBackgroundData = themeCollectionData;
        // Saves the file path to the SharedPreference.
        NtpCustomizationUtils.setBackgroundImageFilePathToSharedPreference(
                NtpCustomizationUtils.getBackgroundImageFileFromPath(
                                themeCollectionData.getLastUploadImageFilePath())
                        .getAbsolutePath());

        onBackgroundImageChanged(
                assumeNonNull(themeCollectionData.getBitmap()),
                themeCollectionData.getBackgroundImageInfo(),
                oldType);
        // Updates the daily refresh timestamp if daily refresh enabled.
        NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                TimeUtils.currentTimeMillis(), mBackgroundType, mCustomBackgroundInfo);

        // This method can be called from 1) the user chooses a theme collection image, or 2) the
        // user chooses a previously selected theme collection image from history. For case 1), we
        // defer the saving of the primary color until the bottom sheet is closed. It will be
        // handled by NtpCustomizationMediator. For case 2), the primary color has been calculated
        // before, save it to the Shared Preference now.
        NtpCustomizationUtils.saveBackgroundInfo(
                themeCollectionData,
                themeCollectionData.isBitmapSaved() ? null : themeCollectionData.getBitmap(),
                assumeNonNull(themeCollectionData.getBackgroundImageInfo()),
                /* skipSavingPrimaryColor= */ true);
    }

    /**
     * Notifies listeners about the NTP's background image change.
     *
     * @param bitmap The new background image bitmap before transformations.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     * @param oldBackgroundType The previous type of the NTP's background.
     */
    @VisibleForTesting
    void onBackgroundImageChanged(
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
     * @param backgroundData : The selected NTP background theme data.
     */
    private void onBackgroundColorChanged(Context context, NtpBackgroundDataBase backgroundData) {
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.getNtpThemeColorInfoFromNtpBackgroundData(backgroundData);
        if (colorInfo == null) return;

        @NtpBackgroundType
        int backgroundType =
                colorInfo instanceof NtpThemeColorFromHexInfo
                        ? NtpBackgroundType.COLOR_FROM_HEX
                        : NtpBackgroundType.CHROME_COLOR;

        // Applies the primary theme color to the activity before calculating the background color
        // which is a themed color depending on the activity's theme.
        if (context instanceof Activity activity) {
            NtpCustomizationUtils.applyDynamicColorToActivity(
                    activity, NtpThemeColorUtils.getPrimaryColorFromColorInfo(context, colorInfo));
        }

        @NtpBackgroundType int oldType = mBackgroundType;
        mBackgroundType = backgroundType;
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(mBackgroundType);

        mNtpBackgroundData = backgroundData;

        if (mBackgroundType == NtpBackgroundType.CHROME_COLOR) {
            cleanupImageInfoAndNotifyBackgroundColorChangeImpl(context, oldType);

            NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(assumeNonNull(colorInfo).id);
            // Updates the daily refresh timestamp if enabled.
            NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                    TimeUtils.currentTimeMillis(),
                    mBackgroundType,
                    /* customBackgroundInfo= */ null);
        }

        if (colorInfo instanceof NtpThemeColorFromHexInfo colorFromHexInfo) {
            cleanupImageInfoAndNotifyBackgroundColorChangeImpl(context, oldType);

            NtpCustomizationUtils.saveThemeColorFromHexInfoToSharedPreference(colorFromHexInfo);
        }
    }

    /**
     * Maybe save the NtpBackgroundDataBase instance to the user selection local history list in the
     * SharedPreference. This will be called when the NTP customization bottom sheet is closed with
     * a new customized NTP theme is selected.
     */
    public void maybeSaveUserSelectedBackgroundTypeToSharedPreference(Context context) {
        if (!mIsNtpCustomizationSyncEnabled || mNtpBackgroundData == null) return;

        if (mNtpBackgroundDataManager == null) {
            mNtpBackgroundDataManager = new NtpBackgroundDataManager(context);
        }
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(
                mNtpBackgroundData);
    }

    private void cleanupImageInfoAndNotifyBackgroundColorChangeImpl(
            Context context, @NtpBackgroundType int oldType) {
        cleanupBackgroundImage(!mIsNtpCustomizationSyncEnabled);
        notifyBackgroundColorChanged(context, /* fromInitialization= */ false, oldType);
    }

    /** Notifies listeners about the NTP's customized background is reset. */
    private void onBackgroundReset() {
        @NtpBackgroundType int oldType = mBackgroundType;
        mBackgroundType = NtpBackgroundType.DEFAULT;
        mNtpBackgroundData = null;

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
                NtpThemeColorUtils.getBackgroundColorFromNtpBackgroundData(
                        context, mNtpBackgroundData);
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundColorChanged(
                    assumeNonNull(
                            NtpThemeColorUtils.getNtpThemeColorInfoFromNtpBackgroundData(
                                    mNtpBackgroundData)),
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
    void notifyBackgroundReset(@NtpBackgroundType int oldType) {
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

    /** Returns the {@link NtpBackgroundDataBase} instance representing the current theme. */
    public @Nullable NtpBackgroundDataBase getNtpBackgroundData() {
        return mNtpBackgroundData;
    }

    /**
     * Returns the current background color for NTP. Needs to use the Activity's context rather than
     * the application's context, which isn't themed and will provide a wrong color.
     *
     * @param context The current Activity context. It is themed and can provide the correct color.
     */
    public @ColorInt int getBackgroundColor(Context context) {
        if (!mIsInitialized || mNtpBackgroundData == null) {
            return NtpThemeColorUtils.getDefaultBackgroundColor(context);
        }

        return NtpThemeColorUtils.getBackgroundColorFromNtpBackgroundData(
                context, mNtpBackgroundData);
    }

    public @Nullable CustomBackgroundInfo getCustomBackgroundInfo() {
        return mCustomBackgroundInfo;
    }

    public @Nullable NtpThemeColorInfo getNtpThemeColorInfo() {
        return NtpThemeColorUtils.getNtpThemeColorInfoFromNtpBackgroundData(mNtpBackgroundData);
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

    public void setNtpBackgroundDataForTesting(@Nullable NtpBackgroundDataBase backgroundData) {
        mNtpBackgroundData = backgroundData;
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
    private void cleanupBackgroundImage(boolean deleteImageFile) {
        mBackgroundImageInfo = null;
        mOriginalBitmap = null;
        NtpCustomizationUtils.resetCustomizedImage(deleteImageFile);
    }

    private void cleanupChromeColors() {
        NtpCustomizationUtils.resetCustomizedColors();
    }

    private void cleanupOnBackgroundTypeChanged(@NtpBackgroundType int oldType) {
        if (oldType == mBackgroundType) return;

        switch (oldType) {
            case CHROME_COLOR -> cleanupChromeColors();
            case IMAGE_FROM_DISK, THEME_COLLECTION ->
                    cleanupBackgroundImage(!mIsNtpCustomizationSyncEnabled);
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
        mNtpBackgroundData = null;
        cleanupBackgroundImage(/* deleteImageFile= */ true);
        mIsMvtToggleOn = false;
    }

    void setCustomBackgroundInfoForTesting(CustomBackgroundInfo customBackgroundInfo) {
        mCustomBackgroundInfo = customBackgroundInfo;
    }

    @Nullable Bitmap getOriginalBitmapForTesting() {
        return mOriginalBitmap;
    }

    public void setNtpBackgroundDataManagerForTesting(NtpBackgroundDataManager manager) {
        mNtpBackgroundDataManager = manager;
    }
}
