// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
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
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeManager;
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

import java.util.Objects;
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
    private @Nullable NtpBackgroundDataBase mSyncedNtpBackgroundData;
    // Whether a synced theme has replaced the persisted theme in this process and hasn't been
    // applied yet. An Activity computes its dynamic colors from the persisted theme when it is
    // created, so it is stale until maybeApplyBackgroundUpdateFromDeviceSync() recreates it. This
    // can't be derived from the in-memory state: a color theme is initialized lazily from
    // SharedPreferences, which the synced theme may have replaced by then.
    private boolean mIsPersistedThemeReplacedBySync;

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
            @Nullable
            @ColorInt
            Integer primaryColor =
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
            NtpThemeDailyRefreshManager ntpThemeDailyRefreshManager =
                    NtpThemeDailyRefreshManager.getInstance();
            CustomBackgroundInfo customBackgroundInfo =
                    ntpThemeDailyRefreshManager.getNtpCustomBackgroundInfoForThemeCollection();
            if (customBackgroundInfo == null) {
                // The theme collection can't be restored if its info is missing or invalid. Clears
                // the persisted background data and falls back to the default background. Unlike
                // onBackgroundInitializationFailed(), doesn't notify any listener since this is
                // called from the constructor, and doesn't delete the image files to avoid disk I/O
                // on the UI thread.
                mBackgroundType = NtpBackgroundType.DEFAULT;
                NtpCustomizationUtils.removeAllNtpBackgroundDataFromSharedPreference();
            } else {
                mIsInitialized = true;
                BackgroundImageInfo imageInfo =
                        ntpThemeDailyRefreshManager.getNtpBackgroundImageInfoForThemeCollection();
                @Nullable
                @ColorInt
                Integer primaryColor =
                        ntpThemeDailyRefreshManager.getNtpThemeColorForThemeCollection();
                ntpThemeDailyRefreshManager.readNtpBackgroundImageForThemeCollection(
                        (bitmap) -> {
                            onBackgroundImageLoadedFromDisk(bitmap, imageInfo);
                        },
                        EXECUTOR,
                        filePath);
                mCustomBackgroundInfo = customBackgroundInfo;
                mNtpBackgroundData =
                        new NtpBackgroundDataThemeCollection(
                                PlatformType.ANDROID,
                                customBackgroundInfo,
                                imageInfo,
                                /* bitmap= */ null,
                                primaryColor,
                                fileIdHash);
            }
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
                onBackgroundInitializationFailed();
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

    /**
     * Ensures color themes ({@link NtpBackgroundType#CHROME_COLOR} and {@link
     * NtpBackgroundType#COLOR_FROM_HEX}) are initialized if not already initialized.
     *
     * @param context The context used to resolve theme color resources.
     */
    public void ensureInitialized(Context context) {
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
            @Nullable
            @ColorInt
            Integer primaryColor =
                    NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference();
            if (primaryColor == null) {
                onBackgroundInitializationFailed();
                return;
            }

            @ColorInt
            int backgroundColor =
                    NtpCustomizationUtils.getBackgroundColorFromSharedPreference(
                            NtpThemeColorUtils.getDefaultBackgroundColor(context));
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
            ensureInitialized(context);
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
        onBackgroundDataChanged(
                context, backgroundData, /* shouldNotifySyncedThemeManager= */ true);
    }

    /**
     * Called when users selected a new NTP background theme, or when an incoming sync update is
     * applied.
     *
     * @param context The application context.
     * @param backgroundData The selected NTP background theme data.
     * @param shouldNotifySyncedThemeManager True if the change should be committed to native via
     *     {@link NtpSyncedThemeManager} (e.g. for local user selection), false if it is from
     *     incoming sync.
     */
    public void onBackgroundDataChanged(
            Context context,
            @Nullable NtpBackgroundDataBase backgroundData,
            boolean shouldNotifySyncedThemeManager) {
        clearPendingSyncedBackgroundAndSharedPreference(context, backgroundData);

        if (backgroundData == null
                || backgroundData.getBackgroundType() == NtpBackgroundType.DEFAULT
                || (backgroundData instanceof NtpBackgroundDataColor ntpBackgroundDataColor
                        && ntpBackgroundDataColor.getThemeColorId() == NtpThemeColorId.DEFAULT)
                || (backgroundData instanceof NtpBackgroundDataImageBase imageBaseData
                        && imageBaseData.getBitmap() == null)) {
            backgroundData = null;
            onBackgroundReset();
        } else {
            // Persists the new background type. The rest of the new background's data is persisted
            // by the type specific handlers below.
            NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                    backgroundData.getBackgroundType());

            if (backgroundData instanceof NtpBackgroundDataColor
                    || backgroundData.getBackgroundType() == NtpBackgroundType.COLOR_FROM_HEX) {
                onBackgroundColorChanged(context, backgroundData);
            } else if (backgroundData instanceof NtpBackgroundDataUploadImage uploadImageData) {
                onUploadedImageSelected(uploadImageData);
            } else if (backgroundData
                    instanceof NtpBackgroundDataThemeCollection themeCollectionData) {
                onThemeCollectionImageSelected(themeCollectionData);
            }
        }

        if (shouldNotifySyncedThemeManager) {
            NtpSyncedThemeManager.getInstance().onThemeCommitted(backgroundData);
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
        mCustomBackgroundInfo = null;
        // Saves the file path to the SharedPreference.
        NtpCustomizationUtils.setBackgroundImageFilePathToSharedPreference(
                NtpCustomizationUtils.getBackgroundImageFileFromPath(
                                uploadImageData.getLastUploadImageFilePath())
                        .getAbsolutePath());

        Bitmap bitmap = assumeNonNull(uploadImageData.getBitmap());
        BackgroundImageInfo backgroundImageInfo =
                assumeNonNull(uploadImageData.getBackgroundImageInfo());

        boolean fromHistoryData = uploadImageData.getPrimaryColor() != null;
        // If this upload image data is selected from history item list, the primary color and image
        // bitmap have been saved to disk before. Thus, we don't need to pick the primary color or
        // save the bitmap again, but only update the current primary color to the Shared
        // Preference.
        NtpCustomizationUtils.saveBackgroundInfo(
                uploadImageData, fromHistoryData ? null : bitmap, backgroundImageInfo);

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

        onBackgroundImageChanged(
                assumeNonNull(themeCollectionData.getBitmap()),
                themeCollectionData.getBackgroundImageInfo(),
                oldType);
        // Updates the daily refresh timestamp if daily refresh enabled.
        NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                TimeUtils.currentTimeMillis(), mBackgroundType, mCustomBackgroundInfo);

        saveThemeCollectionInfoToSharedPreference(themeCollectionData);
    }

    /**
     * Called when a new theme collection image has been received from Chrome Sync. Saves the synced
     * background info and bitmap asynchronously and caches the synced theme data. Ignored if the
     * theme collection is already persisted on this device.
     *
     * <p>Note: We only persist the synced image file path, background type, and background metadata
     * (CustomBackgroundInfo, matrices, and primary color) to SharedPreferences and disk, and cache
     * the synced theme data in {@code mSyncedNtpBackgroundData} here without modifying the active
     * in-memory state ({@code mNtpBackgroundData}). The cached {@code mSyncedNtpBackgroundData} is
     * applied by {@link #maybeApplyBackgroundUpdateFromDeviceSync(Context)} when the NTP becomes
     * active.
     *
     * @param context The context for managing synced background image files.
     * @param themeCollectionData The synced background theme collection data.
     */
    public void onSyncedThemeCollectionImageChanged(
            Context context, NtpBackgroundDataThemeCollection themeCollectionData) {
        if (!maybeClearForSyncedBackground(context, themeCollectionData)) return;

        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.THEME_COLLECTION);
        saveThemeCollectionInfoToSharedPreference(themeCollectionData);

        // Caches the synced theme data to apply it in maybeApplyBackgroundUpdateFromDeviceSync().
        mSyncedNtpBackgroundData = themeCollectionData;
        mIsPersistedThemeReplacedBySync = true;
    }

    /**
     * Called when a new Chrome color theme has been received from Chrome Sync. Ignored if the
     * Chrome color is already persisted on this device.
     *
     * @param context The context for managing background theme resources.
     * @param colorData The synced Chrome color data.
     */
    public void onSyncedChromeColorChanged(Context context, NtpBackgroundDataColor colorData) {
        if (!maybeClearForSyncedBackground(context, colorData)) return;

        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.CHROME_COLOR);
        saveChromeColorInfoToSharedPreference(colorData);

        mSyncedNtpBackgroundData = colorData;
        mIsPersistedThemeReplacedBySync = true;
    }

    /**
     * Called when a theme reset to default has been received from Chrome Sync. Ignored if the
     * default theme is already persisted on this device.
     *
     * @param context The context for managing background theme resources.
     */
    public void onSyncedDefaultThemeReset(Context context) {
        if (!maybeClearForSyncedBackground(context, /* syncedBackgroundData= */ null)) return;

        mIsPersistedThemeReplacedBySync = true;
    }

    /**
     * Clears the pending synced background and SharedPreferences for a synced background, unless it
     * is already persisted on this device. The active in-memory state isn't changed.
     *
     * @param context The context for managing synced background image files.
     * @param syncedBackgroundData The background data received from Chrome Sync, or null for the
     *     default background.
     * @return True if the data was cleared and the synced background should be saved; false if it
     *     is already persisted and nothing was changed.
     */
    private boolean maybeClearForSyncedBackground(
            Context context, @Nullable NtpBackgroundDataBase syncedBackgroundData) {
        if (NtpCustomizationUtils.isSyncedBackgroundPersisted(syncedBackgroundData)) return false;

        clearPendingSyncedBackgroundAndSharedPreference(context, syncedBackgroundData);
        return true;
    }

    /**
     * Clears the background data before a new background is saved:
     *
     * <ul>
     *   <li>The pending synced background in memory, i.e. {@code mSyncedNtpBackgroundData}. Its
     *       image file is cleaned up if unused, unless it is the new background.
     *   <li>All the NTP background data in SharedPreferences, which may belong to the active or the
     *       pending synced background.
     * </ul>
     *
     * @param context The context for managing synced background image files.
     * @param newBackgroundData The new background to save, or null if resetting to default.
     */
    private void clearPendingSyncedBackgroundAndSharedPreference(
            Context context, @Nullable NtpBackgroundDataBase newBackgroundData) {
        // Only cleans up the pending synced background's unused image file if it is replaced by a
        // different background.
        if (mSyncedNtpBackgroundData instanceof NtpBackgroundDataImageBase imageBaseData
                && !Objects.equals(mSyncedNtpBackgroundData, newBackgroundData)) {
            getNtpBackgroundDataManager(context).maybeCleanUpUnusedSyncedImageData(imageBaseData);
        }
        mSyncedNtpBackgroundData = null;
        mIsPersistedThemeReplacedBySync = false;
        NtpCustomizationUtils.removeAllNtpBackgroundDataFromSharedPreference();
        // A daily refresh applied in memory was computed from the background that is being
        // replaced, and must not be saved over the new one at deferred startup.
        NtpThemeDailyRefreshManager.getInstance().discardPendingDailyRefresh();
    }

    /**
     * Persists the theme collection's image file path, background info, matrices and primary color
     * to SharedPreferences, and saves the bitmap to disk if it isn't saved on this device yet (e.g.
     * when newly selected, synced or from remote history).
     */
    private static void saveThemeCollectionInfoToSharedPreference(
            NtpBackgroundDataThemeCollection themeCollectionData) {
        NtpCustomizationUtils.setBackgroundImageFilePathToSharedPreference(
                NtpCustomizationUtils.getBackgroundImageFileFromPath(
                                themeCollectionData.getLastUploadImageFilePath())
                        .getAbsolutePath());
        NtpCustomizationUtils.saveBackgroundInfo(
                themeCollectionData,
                themeCollectionData.isBitmapSaved() ? null : themeCollectionData.getBitmap(),
                assumeNonNull(themeCollectionData.getBackgroundImageInfo()));
    }

    /** Persists the Chrome color's color id and daily refresh setting to SharedPreferences. */
    private static void saveChromeColorInfoToSharedPreference(NtpBackgroundDataColor colorData) {
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(colorData.getThemeColorId());
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(
                colorData.isChromeColorDailyRefreshEnabled());
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
        assert colorInfo != null;

        @NtpBackgroundType
        int backgroundType =
                colorInfo instanceof NtpThemeColorFromHexInfo
                        ? NtpBackgroundType.COLOR_FROM_HEX
                        : NtpBackgroundType.CHROME_COLOR;

        @NtpBackgroundType int oldType = mBackgroundType;
        mBackgroundType = backgroundType;

        mNtpBackgroundData = backgroundData;

        if (mBackgroundType == NtpBackgroundType.CHROME_COLOR) {
            cleanupImageInfoAndNotifyBackgroundColorChangeImpl(context, oldType);

            assert backgroundData instanceof NtpBackgroundDataColor;
            saveChromeColorInfoToSharedPreference((NtpBackgroundDataColor) backgroundData);
            // Updates the daily refresh timestamp if enabled. This must be called after the daily
            // refresh setting is saved, which it reads.
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
     * a new customized NTP theme is selected, or when a synced background theme is applied.
     */
    public void maybeSaveUserSelectedBackgroundTypeToSharedPreference(Context context) {
        if (!mIsNtpCustomizationSyncEnabled || mNtpBackgroundData == null) return;

        getNtpBackgroundDataManager(context)
                .saveUserSelectedBackgroundTypeToSharedPreference(mNtpBackgroundData);
    }

    /**
     * Returns the {@link NtpBackgroundDataManager} instance, creating one lazily if necessary.
     *
     * @param context The context used to instantiate the manager if not already created.
     * @return The {@link NtpBackgroundDataManager} instance.
     */
    private NtpBackgroundDataManager getNtpBackgroundDataManager(Context context) {
        if (mNtpBackgroundDataManager == null) {
            mNtpBackgroundDataManager = new NtpBackgroundDataManager(context);
        }
        return mNtpBackgroundDataManager;
    }

    private void cleanupImageInfoAndNotifyBackgroundColorChangeImpl(
            Context context, @NtpBackgroundType int oldType) {
        cleanupBackgroundImage(oldType);
        notifyBackgroundColorChanged(context, /* fromInitialization= */ false, oldType);
    }

    /**
     * Called when the active background fails to initialize. Resets to the default background. If
     * the persisted background was replaced by sync in the meantime, the SharedPreferences hold the
     * synced background (or nothing for a synced reset) and are kept so that it will be applied by
     * {@link #maybeApplyBackgroundUpdateFromDeviceSync(Context)}.
     */
    private void onBackgroundInitializationFailed() {
        if (!mIsPersistedThemeReplacedBySync) {
            NtpCustomizationUtils.removeAllNtpBackgroundDataFromSharedPreference();
            // The daily refresh which may have been applied to the failed background is gone too.
            NtpThemeDailyRefreshManager.getInstance().discardPendingDailyRefresh();
        }
        onBackgroundReset();
    }

    /**
     * Notifies listeners about the NTP's customized background is reset. The SharedPreferences
     * should be cleaned up by the caller.
     */
    private void onBackgroundReset() {
        @NtpBackgroundType int oldType = mBackgroundType;
        mBackgroundType = NtpBackgroundType.DEFAULT;
        mNtpBackgroundData = null;

        cleanupBackgroundImage(oldType);
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
        NtpThemeColorInfo ntpThemeColorInfo =
                NtpThemeColorUtils.getNtpThemeColorInfoFromNtpBackgroundData(mNtpBackgroundData);
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundColorChanged(
                    assumeNonNull(ntpThemeColorInfo),
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

    /**
     * Cleans up the in-memory background image state when switching to a non-image background. When
     * switching away from an image background, also deletes the image files if sync is disabled.
     * Otherwise, the files may be shared with the theme history lists and are cleaned up by
     * NtpBackgroundDataManager.
     *
     * @param oldType The previous type of the NTP's background.
     */
    private void cleanupBackgroundImage(@NtpBackgroundType int oldType) {
        mBackgroundImageInfo = null;
        mOriginalBitmap = null;
        mCustomBackgroundInfo = null;
        if (mIsNtpCustomizationSyncEnabled) return;

        if (oldType == IMAGE_FROM_DISK || oldType == THEME_COLLECTION) {
            NtpCustomizationUtils.deleteAllNtpBackgroundImageFiles();
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
        mSyncedNtpBackgroundData = null;
        mIsPersistedThemeReplacedBySync = false;
        mBackgroundImageInfo = null;
        mOriginalBitmap = null;
        mCustomBackgroundInfo = null;
        NtpCustomizationUtils.removeAllNtpBackgroundDataFromSharedPreference();
        NtpCustomizationUtils.deleteAllNtpBackgroundImageFiles();
        mIsMvtToggleOn = false;
    }

    /**
     * Called when the NTP comes to the foreground. Applies the synced theme that replaced the
     * persisted theme since the Activity was created, if any, and recreates the Activity so that
     * its dynamic colors follow.
     *
     * @param context The context used for applying background data and saving history.
     */
    public void maybeApplyBackgroundUpdateFromDeviceSync(Context context) {
        if (!mIsInitialized) {
            return;
        }

        if (mIsPersistedThemeReplacedBySync) {
            // Applies the staged synced background, or resets to default if there is none.
            onBackgroundDataChanged(
                    context, mSyncedNtpBackgroundData, /* shouldNotifySyncedThemeManager= */ false);
            maybeSaveUserSelectedBackgroundTypeToSharedPreference(context);
            // Recreate the Activity to safely apply the new image AND the new OS-level dynamic
            // theme colors together.
            if (mNtpThemeStateProvider != null) {
                mNtpThemeStateProvider.notifyApplyThemeChanges();
            }
        }

        mSyncedNtpBackgroundData = null;
        mIsPersistedThemeReplacedBySync = false;
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

    public @Nullable NtpBackgroundDataBase getSyncedNtpBackgroundData() {
        return mSyncedNtpBackgroundData;
    }
}
