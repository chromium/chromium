// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataCustomizedColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataUploadImage;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;

/** Manages the lifecycle of NtpSyncedThemeBridge and observes committed theme changes. */
@NullMarked
public class NtpSyncedThemeManager implements NtpSyncedThemeBridge.Observer {
    private static @Nullable NtpSyncedThemeManager sInstanceForTesting;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final NtpSyncedThemeManager INSTANCE = new NtpSyncedThemeManager();
    }

    /** Returns the singleton instance of {@link NtpSyncedThemeManager}. */
    public static NtpSyncedThemeManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.INSTANCE;
    }

    // The application context. Sync callbacks can arrive while no Activity exists, so this is the
    // only context available. It must not be used to derive a color from the theme, e.g. via
    // NtpThemeColorUtils#getDefaultBackgroundColor(). Building synced background data with it is
    // fine for now: an NtpBackgroundDataColor is only consumed by its color id and
    // primaryColorResId, and an NtpBackgroundDataCustomizedColor arrives from sync with all four
    // colors already generated natively (see CrossDeviceThemeTracker#createCustomizedColorData).
    // TODO(crbug.com/488439751): Build the synced background data with the Activity context it
    // is applied with, so that this doesn't depend on how the data is consumed.
    private final Context mContext;
    private final NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    // The profile this manager is attached to. Only used to skip re-attaching to the same profile.
    private @Nullable Profile mProfile;
    private @Nullable ImageFetcher mImageFetcher;
    private @Nullable NtpSyncedThemeBridge mNtpSyncedThemeBridge;
    // Tracks the URL of the most recently requested synced background image to prevent race
    // conditions (e.g. if a newer Chrome color or default reset arrives while the image download is
    // in-flight, or if multiple sync updates arrive in rapid succession).
    // TODO(crbug.com/488439751): Handle when a user manually selects an image while a synced image
    // download is in-flight, so the in-flight download does not overwrite the user's manual
    // selection when completed.
    private @Nullable String mLatestSyncedBackgroundUrl;
    // Tracks whether an image is actively downloading via mImageFetcher.
    private boolean mIsImageDownloading;
    // Callbacks waiting for the theme image downloading to complete.
    private final ObserverList<Callback<@Nullable NtpBackgroundDataThemeCollection>>
            mDownloadCallbacks = new ObserverList<>();

    private NtpSyncedThemeManager() {
        this(NtpCustomizationConfigManager.getInstance());
    }

    /**
     * Creates a manager that isn't attached to any profile yet; see {@link
     * #onFinishNativeInitialization(Profile)}.
     *
     * @param ntpCustomizationConfigManager The config manager which receives the synced themes and
     *     reports the locally committed ones. Injected so tests can pass a spy or a mock.
     */
    @VisibleForTesting
    NtpSyncedThemeManager(NtpCustomizationConfigManager ntpCustomizationConfigManager) {
        mContext = ContextUtils.getApplicationContext();
        mNtpCustomizationConfigManager = ntpCustomizationConfigManager;
    }

    /**
     * Attaches the manager to the given profile: creates the {@link NtpSyncedThemeBridge} and the
     * {@link ImageFetcher}. Calling this again with the same profile is a no-op; calling it with a
     * different profile releases the previous profile's state first.
     *
     * @param profile The original (non-incognito) profile.
     */
    public void onFinishNativeInitialization(Profile profile) {
        // The manager is attached on tablets regardless of the feature. Both the feature and the
        // policy are fixed for the lifetime of the process.
        if (!NtpCustomizationUtils.isNtpThemeCustomizationEnabled()) return;

        if (mProfile == profile) return;

        destroy();
        mProfile = profile;
        mImageFetcher = NtpCustomizationUtils.createImageFetcher(profile);
        mNtpSyncedThemeBridge = new NtpSyncedThemeBridge(profile, this);
        // The singleton outlives a test, so detach it between tests. Otherwise the next test's
        // activity would skip the attach above and keep the previous test's bridge.
        ResettersForTesting.register(this::destroy);
    }

    /**
     * Releases the profile-dependent state: the C++ side of {@link NtpSyncedThemeBridge}, the
     * {@link ImageFetcher}, and any in-flight download bookkeeping.
     */
    public void destroy() {
        if (mNtpSyncedThemeBridge != null) {
            mNtpSyncedThemeBridge.destroy();
            mNtpSyncedThemeBridge = null;
        }
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }
        mProfile = null;
        mLatestSyncedBackgroundUrl = null;
        mDownloadCallbacks.clear();
        mIsImageDownloading = false;
    }

    /**
     * Called after a daily refresh for a theme collection is applied. This triggers fetching the
     * image for the next day's refresh if one hasn't been fetched already.
     */
    public void fetchNextThemeCollectionImageAfterDailyRefreshApplied() {
        if (NtpCustomizationUtils.getNtpBackgroundType() != THEME_COLLECTION) {
            return;
        }

        CustomBackgroundInfo customBackgroundInfo =
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference();
        if (customBackgroundInfo == null || !customBackgroundInfo.isDailyRefreshEnabled) {
            return;
        }

        if (NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference()
                != null) {
            return;
        }

        if (mNtpSyncedThemeBridge != null) {
            mNtpSyncedThemeBridge.fetchNextThemeCollectionImage();
        }
    }

    /**
     * Called when the theme collection background in native preferences was updated, either by sync
     * or by the local daily refresh pre-fetching the next image.
     *
     * @param info The {@link CustomBackgroundInfo} containing custom background info.
     * @param primaryColor The synced primary color, or {@code null} if none was provided.
     * @param isImageFromSync True if the background arrived from sync; false if it is the next
     *     image pre-fetched for the local daily refresh.
     */
    @Override
    public void onThemeCollectionImageAvailable(
            @Nullable CustomBackgroundInfo info,
            @Nullable @ColorInt Integer primaryColor,
            boolean isImageFromSync) {
        if (info == null
                || !info.backgroundUrl.isValid()
                || info.backgroundUrl.isEmpty()
                || mImageFetcher == null) {
            return;
        }

        mLatestSyncedBackgroundUrl = info.backgroundUrl.getSpec();
        mIsImageDownloading = isImageFromSync;
        NtpCustomizationUtils.fetchThemeCollectionImage(
                mImageFetcher,
                info.backgroundUrl,
                (bitmap) ->
                        handleFetchedThemeCollectionImage(
                                info, bitmap, primaryColor, isImageFromSync));
    }

    /**
     * Called when a Chrome color has arrived from sync.
     *
     * @param colorId The synced color ID.
     */
    @Override
    public void onChromeColorSynced(int colorId) {
        if (colorId <= NtpThemeColorId.DEFAULT || colorId >= NtpThemeColorId.NUM_ENTRIES) {
            onDefaultThemeSynced();
            return;
        }

        mLatestSyncedBackgroundUrl = null;

        NtpBackgroundDataColor colorData =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        colorId,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpCustomizationConfigManager.onSyncedChromeColorChanged(mContext, colorData);
    }

    /** Called when the theme is reset to default from sync. */
    @Override
    public void onDefaultThemeSynced() {
        mLatestSyncedBackgroundUrl = null;
        mNtpCustomizationConfigManager.onSyncedDefaultThemeReset(mContext);
    }

    /**
     * Called by {@link NtpCustomizationConfigManager} when an NTP theme change is committed
     * locally, to write it to native for outbound sync.
     *
     * @param data The newly chosen background data, or {@code null} for a reset to default.
     */
    public void onThemeCommitted(@Nullable NtpBackgroundDataBase data) {
        if (mNtpSyncedThemeBridge == null) {
            return;
        }

        // 2. Default Chrome Theme
        if (data == null || data.getBackgroundType() == NtpBackgroundType.DEFAULT) {
            mNtpSyncedThemeBridge.resetCustomBackgroundInfo();
            return;
        }

        // 3. Chrome Color Theme
        if (data instanceof NtpBackgroundDataColor colorData) {
            int colorId = colorData.getThemeColorId();
            if (colorId <= NtpThemeColorId.DEFAULT || colorId >= NtpThemeColorId.NUM_ENTRIES) {
                // A color id outside the valid range means "no Chrome color", so commit a reset
                // instead. Mirrors NtpCustomizationConfigManager#onBackgroundDataChanged, which
                // routes NtpThemeColorId.DEFAULT to onBackgroundReset().
                mNtpSyncedThemeBridge.resetCustomBackgroundInfo();
                return;
            }
            mNtpSyncedThemeBridge.setChromeColor(colorId);
            return;
        }

        // 3b. Custom Hex Color Theme
        // TODO(crbug.com/488439751): Support syncing custom hex colors across devices.
        if (data instanceof NtpBackgroundDataCustomizedColor
                || data.getBackgroundType() == NtpBackgroundType.COLOR_FROM_HEX) {
            return;
        }

        // 4. Local Uploaded Image
        if (data instanceof NtpBackgroundDataUploadImage) {
            mNtpSyncedThemeBridge.selectLocalBackgroundImage();
            return;
        }

        // 5. Theme Collection Image (From Picker or Local History)
        if (data instanceof NtpBackgroundDataThemeCollection collectionData) {
            CustomBackgroundInfo info = collectionData.getCustomBackgroundInfo();
            mNtpSyncedThemeBridge.updateCustomBackgroundPrefsWithColor(
                    info.backgroundUrl,
                    info.collectionId,
                    info.attributionLine1,
                    info.attributionLine2,
                    info.attributionActionUrl,
                    collectionData.getPrimaryColor(),
                    info.isDailyRefreshEnabled);
            return;
        }
    }

    /**
     * Handles the downloaded theme collection bitmap image and applies or pre-fetches it based on
     * the sync and daily refresh state.
     *
     * @param info The {@link CustomBackgroundInfo} containing theme collection metadata.
     * @param bitmap The fetched background image bitmap, or {@code null} if downloading failed.
     * @param syncedPrimaryColor The primary color provided by sync, or {@code null} if not present.
     * @param isImageFromSync True if the image arrived from sync; false if it was pre-fetched for
     *     the local daily refresh.
     */
    private void handleFetchedThemeCollectionImage(
            CustomBackgroundInfo info,
            @Nullable Bitmap bitmap,
            @Nullable @ColorInt Integer syncedPrimaryColor,
            boolean isImageFromSync) {
        mIsImageDownloading = false;
        if (bitmap == null) {
            if (isImageFromSync) {
                notifyDownloadCallbacks(null);
            }
            return;
        }

        if (!info.backgroundUrl.getSpec().equals(mLatestSyncedBackgroundUrl)) {
            return;
        }

        BackgroundImageInfo backgroundImageInfo =
                NtpCustomizationUtils.getDefaultBackgroundImageInfo(mContext, bitmap);

        @Nullable NtpBackgroundDataThemeCollection themeCollectionData = null;
        if (!isImageFromSync) {
            // Case 1: Local device next-day daily refresh pre-fetch.
            NtpCustomizationUtils.saveDailyRefreshBackgroundInfo(info, bitmap, backgroundImageInfo);
            return;
        }

        if (info.isDailyRefreshEnabled) {
            // Case 3: Synced daily refresh setup from another device.
            // TODO(crbug.com/488439751): For synced theme collection daily updates,
            // applying the fetched image on the next NTP launch, saving the
            // background info, and subsequently pre-fetching the following day's
            // image will be implemented in a follow-up.
        } else {
            // Case 2: Synced static theme collection image from another device.
            String fileId = NtpCustomizationUtils.getFileName(info.backgroundUrl.getPath());
            themeCollectionData =
                    new NtpBackgroundDataThemeCollection(
                            PlatformType.ANDROID,
                            info,
                            backgroundImageInfo,
                            bitmap,
                            syncedPrimaryColor,
                            /* fileIdHash= */ fileId);
            mNtpCustomizationConfigManager.onSyncedThemeCollectionImageChanged(
                    mContext, themeCollectionData);
        }
        notifyDownloadCallbacks(themeCollectionData);
    }

    /**
     * Notifies all registered download callbacks with the downloaded theme collection data (or
     * {@code null} on failure/omission). All callbacks are cleared after being invoked.
     *
     * @param themeCollectionData The downloaded theme collection data, or null.
     */
    private void notifyDownloadCallbacks(
            @Nullable NtpBackgroundDataThemeCollection themeCollectionData) {
        for (Callback<@Nullable NtpBackgroundDataThemeCollection> callback : mDownloadCallbacks) {
            callback.onResult(themeCollectionData);
        }
        mDownloadCallbacks.clear();
    }

    /**
     * Registers a one-shot callback to be invoked when the theme image download completes. If
     * nothing is downloading, the callback is invoked immediately with {@code null}.
     *
     * @param callback The callback receiving the downloaded theme collection data, or null.
     */
    public void addOneShotCompletionCallback(
            Callback<@Nullable NtpBackgroundDataThemeCollection> callback) {
        if (!isImageDownloading()) {
            callback.onResult(null);
            return;
        }
        mDownloadCallbacks.addObserver(callback);
    }

    /**
     * Removes a previously registered completion callback.
     *
     * @param callback The callback to remove.
     */
    public void removeCompletionCallback(
            Callback<@Nullable NtpBackgroundDataThemeCollection> callback) {
        mDownloadCallbacks.removeObserver(callback);
    }

    /** Returns whether a theme image is currently downloading. */
    public boolean isImageDownloading() {
        return mIsImageDownloading;
    }

    public static void setInstanceForTesting(@Nullable NtpSyncedThemeManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public void setImageDownloadingForTesting(boolean isDownloading) {
        mIsImageDownloading = isDownloading;
    }
}
