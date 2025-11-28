// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
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
    private final Runnable mOnThemeImageSelectedCallback;
    private final @Nullable ImageFetcher mImageFetcher;

    /**
     * Constructs a new NtpThemeCollectionManager.
     *
     * @param context The application context.
     * @param profile The profile for which the {@link NtpThemeCollectionBridge} is created.
     * @param onThemeImageSelectedCallback The callback to run when a theme image is selected.
     */
    public NtpThemeCollectionManager(
            Context context, Profile profile, Runnable onThemeImageSelectedCallback) {
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
        if (mNtpCustomizationConfigManager.getBackgroundImageType() != THEME_COLLECTION) {
            return null;
        }

        CustomBackgroundInfo info = mNtpCustomizationConfigManager.getCustomBackgroundInfo();
        return info != null ? info.collectionId : null;
    }

    public @Nullable GURL getSelectedThemeCollectionImageUrl() {
        if (mNtpCustomizationConfigManager.getBackgroundImageType() != THEME_COLLECTION) {
            return null;
        }

        CustomBackgroundInfo info = mNtpCustomizationConfigManager.getCustomBackgroundInfo();
        return info != null ? info.backgroundUrl : null;
    }

    public boolean getIsDailyRefreshEnabled() {
        if (mNtpCustomizationConfigManager.getBackgroundImageType() != THEME_COLLECTION) {
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
        mNtpThemeCollectionBridge.setThemeCollectionImage(image);
    }

    /** Sets the user-uploaded background image. */
    public void selectLocalBackgroundImage() {
        mNtpThemeCollectionBridge.selectLocalBackgroundImage();
    }

    /** Resets the custom background. */
    public void resetCustomBackground() {
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
                    if (bitmap != null) {
                        mNtpCustomizationConfigManager.onThemeCollectionImageSelected(
                                bitmap,
                                info,
                                NtpCustomizationUtils.calculateInitialThemeCollectionImageMatrices(
                                        mContext, bitmap));
                        mOnThemeImageSelectedCallback.run();
                    }
                });
    }

    /**
     * Sets the New Tab Page background to a theme collection with daily refresh function enabled.
     *
     * @param themeCollectionId The id of the theme collection
     */
    public void setThemeCollectionDailyRefreshed(String themeCollectionId) {
        mNtpThemeCollectionBridge.setThemeCollectionDailyRefreshed(themeCollectionId);
    }
}
