// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
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
    private boolean mIsDestroyed;
    private @Nullable Runnable mFetchNextImageRunnable;
    private @Nullable CollectionImage mSelectingThemeCollectionImage;

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

    /** Cleans up the C++ side of {@link NtpThemeCollectionBridge}. */
    public void destroy() {
        mNtpThemeCollectionBridge.destroy();
        mIsDestroyed = true;
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
        resetSelectionState();
        mSelectingThemeCollectionImage = image;
        mNtpThemeCollectionBridge.setThemeCollectionImage(image);
    }

    /** Sets the user-uploaded background image. */
    public void selectLocalBackgroundImage() {
        resetSelectionState();
        mNtpThemeCollectionBridge.selectLocalBackgroundImage();
    }

    /** Resets the custom background. */
    public void resetCustomBackground() {
        resetSelectionState();
        mNtpThemeCollectionBridge.resetCustomBackground();
    }

    /**
     * Callback from native code, triggered when the custom background image has been successfully
     * updated. This can occur after a new theme is selected.
     *
     * @param info The updated {@link CustomBackgroundInfo}.
     */
    @VisibleForTesting
    public void onCustomBackgroundImageUpdated(@Nullable CustomBackgroundInfo info) {
        if (mImageFetcher == null
                || info == null
                || !info.backgroundUrl.isValid()
                || info.backgroundUrl.isEmpty()) {
            return;
        }

        NtpCustomizationUtils.fetchThemeCollectionImage(
                mImageFetcher,
                info.backgroundUrl,
                (bitmap) -> {
                    if (bitmap == null) {
                        return;
                    }

                    BackgroundImageInfo backgroundImageInfo =
                            NtpCustomizationUtils.getDefaultBackgroundImageInfo(mContext, bitmap);

                    if (isNextThemeCollectionImage(info)) {
                        NtpCustomizationUtils.saveDailyRefreshBackgroundInfo(
                                info, bitmap, backgroundImageInfo);
                        return;
                    }

                    // We do not set the theme collection image as the background if the bottom
                    // sheet is dismissed; this is done to ensure proper theme color handling. Note
                    // that this does not affect the prepared daily refresh image, as we only save
                    // the primary color for that case.
                    if (mIsDestroyed) {
                        return;
                    }

                    if (!shouldProcessThemeUpdate(info)) {
                        return;
                    }

                    mNtpCustomizationConfigManager.onThemeCollectionImageSelected(
                            bitmap, info, backgroundImageInfo);
                    mOnThemeImageSelectedCallback.onResult(bitmap);
                    NtpCustomizationUtils.saveBackgroundInfo(
                            info, bitmap, backgroundImageInfo, /* skipSavingPrimaryColor= */ true);

                    if (mFetchNextImageRunnable != null) {
                        mFetchNextImageRunnable.run();
                        mFetchNextImageRunnable = null;
                    }
                });
    }

    /**
     * Sets the New Tab Page background to a theme collection with daily refresh function enabled.
     *
     * @param themeCollectionId The id of the theme collection
     */
    public void setThemeCollectionDailyRefreshed(String themeCollectionId) {
        resetSelectionState();
        mFetchNextImageRunnable = this::fetchNextThemeCollectionImage;
        mNtpThemeCollectionBridge.setThemeCollectionDailyRefreshed(themeCollectionId);
    }

    /** Fetches the next image for a theme collection with daily refresh enabled. */
    private void fetchNextThemeCollectionImage() {
        mNtpThemeCollectionBridge.fetchNextThemeCollectionImage();
    }

    /**
     * Determines if the updated background information is for the next daily refresh image.
     *
     * <p>This is true if the new image belongs to the same collection as the current one and daily
     * refresh is enabled for both.
     *
     * @param info The incoming {@link CustomBackgroundInfo}.
     */
    private boolean isNextThemeCollectionImage(CustomBackgroundInfo info) {
        if (mNtpCustomizationConfigManager.getBackgroundType() != THEME_COLLECTION) {
            return false;
        }

        CustomBackgroundInfo currentInfo = mNtpCustomizationConfigManager.getCustomBackgroundInfo();
        if (currentInfo == null) {
            return false;
        }

        return currentInfo.isDailyRefreshEnabled
                && info.isDailyRefreshEnabled
                && currentInfo.collectionId.equals(info.collectionId);
    }

    /** Clears the state related to pending theme selections. */
    private void resetSelectionState() {
        mFetchNextImageRunnable = null;
        mSelectingThemeCollectionImage = null;
    }

    /**
     * Determines whether the received theme update should be processed.
     *
     * @param info The {@link CustomBackgroundInfo} of the update.
     */
    private boolean shouldProcessThemeUpdate(CustomBackgroundInfo info) {
        if (mSelectingThemeCollectionImage != null) {
            return mSelectingThemeCollectionImage.imageUrl.equals(info.backgroundUrl);
        }

        return info.isDailyRefreshEnabled;
    }
}
