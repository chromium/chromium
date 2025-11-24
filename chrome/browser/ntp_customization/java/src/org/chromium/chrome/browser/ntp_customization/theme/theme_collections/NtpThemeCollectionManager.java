// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
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
    /** An interface to get theme collection updates. */
    public interface ThemeCollectionSelectionListener {
        /**
         * Called when the user selects a new theme collection image.
         *
         * @param themeCollectionId The ID of the theme collection the image belongs to.
         * @param themeCollectionImageUrl The URL of the selected theme collection image.
         */
        default void onThemeCollectionSelectionChanged(
                @Nullable String themeCollectionId, @Nullable GURL themeCollectionImageUrl) {}
    }

    private final NtpThemeCollectionBridge mNtpThemeCollectionBridge;
    private final Context mContext;
    private final NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private final Runnable mOnThemeImageSelectedCallback;
    private final ObserverList<ThemeCollectionSelectionListener> mThemeCollectionSelectionListeners;
    private final @Nullable ImageFetcher mImageFetcher;

    // Whether the theme collection that the user has currently chosen is daily refresh enabled.
    private boolean mIsDailyRefreshEnabled;
    // The theme collection that the user has currently chosen.
    private @Nullable String mSelectedThemeCollectionId;
    // The theme collection image that the user has currently chosen.
    private @Nullable GURL mSelectedThemeCollectionImageUrl;

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
        mThemeCollectionSelectionListeners = new ObserverList<>();
        mOnThemeImageSelectedCallback = onThemeImageSelectedCallback;
        mImageFetcher = NtpCustomizationUtils.createImageFetcher(profile);
        mNtpCustomizationConfigManager = NtpCustomizationConfigManager.getInstance();

        if (mNtpCustomizationConfigManager.getBackgroundImageType()
                != NtpBackgroundImageType.THEME_COLLECTION) {
            return;
        }

        CustomBackgroundInfo customBackgroundInfo =
                mNtpCustomizationConfigManager.getCustomBackgroundInfo();
        if (customBackgroundInfo != null) {
            updateSelectedThemeCollection(
                    customBackgroundInfo.collectionId, customBackgroundInfo.backgroundUrl);
            mIsDailyRefreshEnabled = customBackgroundInfo.isDailyRefreshEnabled;
        }
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
        return mSelectedThemeCollectionId;
    }

    public @Nullable GURL getSelectedThemeCollectionImageUrl() {
        return mSelectedThemeCollectionImageUrl;
    }

    public boolean getIsDailyRefreshEnabled() {
        return mIsDailyRefreshEnabled;
    }

    /**
     * Updates the currently selected theme collection image in the theme collections bottom sheets.
     *
     * @param themeCollectionId The ID of the theme collection.
     * @param themeCollectionImageUrl The URL of the theme collection image.
     */
    public void updateSelectedThemeCollection(
            @Nullable String themeCollectionId, @Nullable GURL themeCollectionImageUrl) {
        mSelectedThemeCollectionId = themeCollectionId;
        mSelectedThemeCollectionImageUrl = themeCollectionImageUrl;

        for (ThemeCollectionSelectionListener listener : mThemeCollectionSelectionListeners) {
            listener.onThemeCollectionSelectionChanged(
                    mSelectedThemeCollectionId, mSelectedThemeCollectionImageUrl);
        }
    }

    /**
     * Adds a {@link ThemeCollectionSelectionListener} to receive updates when the theme collection
     * selection changes.
     */
    public void addListener(ThemeCollectionSelectionListener listener) {
        mThemeCollectionSelectionListeners.addObserver(listener);
    }

    /**
     * Removes the given listener from the selection listener list.
     *
     * @param listener The listener to remove.
     */
    public void removeListener(ThemeCollectionSelectionListener listener) {
        mThemeCollectionSelectionListeners.removeObserver(listener);
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

        // TODO(crbug.com/423579377): Move the entire block of code to after the bitmap check, once
        // the bitmap filter has been set in ntp_background_data.cc.
        updateSelectedThemeCollection(info.collectionId, info.backgroundUrl);
        // TODO(crbug.com/423579377): update(turn on or turn off) daily update button if the current
        // page is that particular single theme collection bottom sheet.
        mIsDailyRefreshEnabled = info.isDailyRefreshEnabled;
        mOnThemeImageSelectedCallback.run();

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
                    }
                });
    }

    public NtpThemeCollectionBridge getNtpThemeCollectionBridgeForTesting() {
        return mNtpThemeCollectionBridge;
    }
}
