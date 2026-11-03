// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Manages the lifecycle of {@link NtpThemeCollectionBridge} and drives NTP theme changes. This
 * object exists only if a NTP customization bottom sheet is visible. It is responsible only for
 * handling manual settings and changing the NTP background/NTP customization bottom sheet according
 * to the new theme collection selected.
 */
@NullMarked
public class NtpThemeCollectionManager {

    private final NtpThemeCollectionBridge mNtpThemeCollectionBridge;
    private final Context mContext;
    private final NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private final Callback<Bitmap> mOnThemeImageSelectedCallback;
    private final @Nullable ImageFetcher mImageFetcher;
    private boolean mHasActiveSelection;
    private @Nullable Runnable mFetchNextImageRunnable;
    // Cancels the callback of the in-flight download when a newer selection starts, or when the
    // current selection is canceled or destroyed, so that any stale result is dropped when it
    // arrives.
    private @Nullable CallbackController mDownloadCallbackController;

    /**
     * Constructs a new NtpThemeCollectionManager.
     *
     * @param context The application context.
     * @param profile The profile for which the {@link NtpThemeCollectionBridge} is created.
     * @param onThemeImageSelectedCallback The callback to run when a theme image is selected.
     */
    public NtpThemeCollectionManager(
            Context context, Profile profile, Callback<Bitmap> onThemeImageSelectedCallback) {
        mContext = context;
        mNtpThemeCollectionBridge =
                new NtpThemeCollectionBridge(profile, this::onCustomBackgroundImageUpdated);
        mOnThemeImageSelectedCallback = onThemeImageSelectedCallback;
        mImageFetcher = NtpCustomizationUtils.createImageFetcher(profile);
        mNtpCustomizationConfigManager = NtpCustomizationConfigManager.getInstance();
    }

    /**
     * Cleans up the C++ side of {@link NtpThemeCollectionBridge} and cancels any in-flight
     * download.
     */
    public void destroy() {
        mNtpThemeCollectionBridge.destroy();
        cancelPendingSelection();
    }

    /**
     * Fetches the list of background collections.
     *
     * @param callback The callback to be invoked with the list of collections.
     */
    public void getBackgroundCollections(Callback<@Nullable List<BackgroundCollection>> callback) {
        mNtpThemeCollectionBridge.getBackgroundCollections(callback);
    }

    /**
     * Fetches the list of images for a given collection.
     *
     * @param collectionId The ID of the collection to fetch images from.
     * @param callback The callback to be invoked with the list of images.
     */
    public void getBackgroundImages(
            String collectionId, Callback<@Nullable List<CollectionImage>> callback) {
        mNtpThemeCollectionBridge.getBackgroundImages(collectionId, callback);
    }

    public @Nullable String getSelectedThemeCollectionId() {
        if (mNtpCustomizationConfigManager.getBackgroundType() != THEME_COLLECTION) {
            return null;
        }

        CustomBackgroundInfo info = mNtpCustomizationConfigManager.getCustomBackgroundInfo();
        return info != null ? info.collectionId : null;
    }

    public @Nullable GURL getSelectedThemeCollectionImageUrl() {
        if (mNtpCustomizationConfigManager.getBackgroundType() != THEME_COLLECTION) {
            return null;
        }

        CustomBackgroundInfo info = mNtpCustomizationConfigManager.getCustomBackgroundInfo();
        return info != null ? info.backgroundUrl : null;
    }

    public boolean getIsDailyRefreshEnabled() {
        if (mNtpCustomizationConfigManager.getBackgroundType() != THEME_COLLECTION) {
            return false;
        }

        CustomBackgroundInfo info = mNtpCustomizationConfigManager.getCustomBackgroundInfo();
        return info != null && info.isDailyRefreshEnabled;
    }

    /**
     * Sets the New Tab Page background to a specific image from a theme collection.
     *
     * @param image The {@link CollectionImage} selected by the user.
     */
    public void setThemeCollectionImage(CollectionImage image) {
        cancelPendingSelection();
        mHasActiveSelection = true;
        mNtpThemeCollectionBridge.setThemeCollectionImage(image);
    }

    /** Cancels any in-flight theme collection selection or download. */
    public void cancelPendingSelection() {
        mHasActiveSelection = false;
        mFetchNextImageRunnable = null;
        resetDownloadCallbackController();
    }

    /**
     * Callback from native code, triggered when the custom background image has been successfully
     * updated. This can occur after a new theme is selected.
     *
     * @param info The updated {@link CustomBackgroundInfo}.
     */
    @VisibleForTesting
    public void onCustomBackgroundImageUpdated(@Nullable CustomBackgroundInfo info) {
        if (!mHasActiveSelection
                || mImageFetcher == null
                || info == null
                || !info.backgroundUrl.isValid()
                || info.backgroundUrl.isEmpty()) {
            return;
        }

        resetDownloadCallbackController();
        mDownloadCallbackController = new CallbackController();
        NtpCustomizationUtils.fetchThemeCollectionImage(
                mImageFetcher,
                info.backgroundUrl,
                mDownloadCallbackController.makeCancelable(
                        (bitmap) -> handleFetchedThemeCollectionImage(info, bitmap)));
    }

    /**
     * Applies the downloaded theme collection bitmap image. A superseded, canceled, or destroyed
     * download is canceled via {@link #mDownloadCallbackController} before reaching this method.
     *
     * @param info The {@link CustomBackgroundInfo} containing theme collection metadata.
     * @param bitmap The fetched background image bitmap, or {@code null} if downloading failed.
     */
    private void handleFetchedThemeCollectionImage(
            CustomBackgroundInfo info, @Nullable Bitmap bitmap) {
        if (bitmap == null) {
            mFetchNextImageRunnable = null;
            return;
        }

        BackgroundImageInfo backgroundImageInfo =
                NtpCustomizationUtils.getDefaultBackgroundImageInfo(mContext, bitmap);

        String fileId = null;
        if (NtpCustomizationUtils.isNTPCustomizationSyncEnabled()) {
            fileId = NtpCustomizationUtils.getFileName(info.backgroundUrl.getPath());
        }
        NtpBackgroundDataThemeCollection backgroundData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        backgroundImageInfo,
                        bitmap,
                        /* primaryColor= */ null,
                        fileId);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        mOnThemeImageSelectedCallback.onResult(bitmap);

        if (mFetchNextImageRunnable != null) {
            mFetchNextImageRunnable.run();
            mFetchNextImageRunnable = null;
        }
    }

    /**
     * Sets the New Tab Page background to a theme collection with daily refresh function enabled.
     *
     * @param themeCollectionId The id of the theme collection
     */
    public void setThemeCollectionDailyRefreshed(String themeCollectionId) {
        cancelPendingSelection();
        mHasActiveSelection = true;
        mFetchNextImageRunnable = mNtpThemeCollectionBridge::fetchNextThemeCollectionImage;
        mNtpThemeCollectionBridge.setThemeCollectionDailyRefreshed(themeCollectionId);
    }

    /** Cancels and clears the {@link CallbackController} for the in-flight download, if any. */
    private void resetDownloadCallbackController() {
        if (mDownloadCallbackController != null) {
            mDownloadCallbackController.destroy();
            mDownloadCallbackController = null;
        }
    }
}
