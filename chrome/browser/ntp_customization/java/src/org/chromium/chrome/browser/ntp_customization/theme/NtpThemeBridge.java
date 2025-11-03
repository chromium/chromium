// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CollectionImage;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** The JNI bridge that deal with theme collections for the NTP. */
@NullMarked
public class NtpThemeBridge {

    private final Runnable mOnThemeImageSelectedCallback;
    private final ObserverList<ThemeCollectionSelectionListener> mThemeCollectionSelectionListeners;
    private long mNativeNtpThemeBridge;

    // Whether the theme collection that the user has currently chosen is daily refresh enabled.
    private boolean mIsDailyRefreshEnabled;
    // The theme collection that the user has currently chosen.
    private @Nullable String mSelectedThemeCollectionId;
    // The theme collection image that the user has currently chosen.
    private @Nullable GURL mSelectedThemeCollectionImageUrl;

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

    /**
     * Constructs a new NtpThemeBridge.
     *
     * @param profile The profile for which this bridge is created.
     * @param onThemeImageSelectedCallback The callback to run when a theme image is selected.
     */
    public NtpThemeBridge(Profile profile, Runnable onThemeImageSelectedCallback) {
        mNativeNtpThemeBridge = NtpThemeBridgeJni.get().init(profile, this);
        mThemeCollectionSelectionListeners = new ObserverList<>();
        mOnThemeImageSelectedCallback = onThemeImageSelectedCallback;

        if (mNativeNtpThemeBridge == 0) return;

        CustomBackgroundInfo customBackgroundInfo =
                NtpThemeBridgeJni.get().getCustomBackgroundInfo(mNativeNtpThemeBridge);
        if (customBackgroundInfo == null
                || !customBackgroundInfo.backgroundUrl.isValid()
                || customBackgroundInfo.backgroundUrl.isEmpty()) {
            return;
        }

        setSelectedTheme(customBackgroundInfo.collectionId, customBackgroundInfo.backgroundUrl);
        mIsDailyRefreshEnabled = customBackgroundInfo.isDailyRefreshEnabled;
    }

    /** Cleans up the C++ side of this class. */
    public void destroy() {
        assert mNativeNtpThemeBridge != 0;
        NtpThemeBridgeJni.get().destroy(mNativeNtpThemeBridge);
        mNativeNtpThemeBridge = 0;
    }

    /**
     * Fetches the list of background collections.
     *
     * @param callback The callback to be invoked with the list of collections.
     */
    public void getBackgroundCollections(Callback<@Nullable List<BackgroundCollection>> callback) {
        NtpThemeBridgeJni.get()
                .getBackgroundCollections(
                        mNativeNtpThemeBridge,
                        new Callback<Object[]>() {
                            @Override
                            public void onResult(Object[] collections) {
                                if (collections == null) {
                                    callback.onResult(null);
                                    return;
                                }
                                List<BackgroundCollection> collectionList = new ArrayList<>();
                                for (Object o : collections) {
                                    assert (o instanceof BackgroundCollection);

                                    collectionList.add((BackgroundCollection) o);
                                }
                                callback.onResult(collectionList);
                            }
                        });
    }

    /**
     * Fetches the list of images for a given collection.
     *
     * @param collectionId The ID of the collection to fetch images from.
     * @param callback The callback to be invoked with the list of images.
     */
    public void getBackgroundImages(
            String collectionId, Callback<@Nullable List<CollectionImage>> callback) {
        NtpThemeBridgeJni.get()
                .getBackgroundImages(
                        mNativeNtpThemeBridge,
                        collectionId,
                        new Callback<Object[]>() {
                            @Override
                            public void onResult(Object[] images) {
                                if (images == null) {
                                    callback.onResult(null);
                                    return;
                                }
                                List<CollectionImage> imageList = new ArrayList<>();
                                for (Object o : images) {
                                    assert (o instanceof CollectionImage);

                                    imageList.add((CollectionImage) o);
                                }
                                callback.onResult(imageList);
                            }
                        });
    }

    /**
     * Creates a {@link BackgroundCollection} object from native.
     *
     * @param id The ID of the collection.
     * @param label The name of the collection.
     * @param previewImageUrl The URL of a preview image for the collection.
     * @return A new {@link BackgroundCollection} object.
     */
    @CalledByNative
    static BackgroundCollection createCollection(String id, String label, GURL previewImageUrl) {
        return new BackgroundCollection(id, label, previewImageUrl);
    }

    /**
     * Creates a {@link CollectionImage} object from native.
     *
     * @param collectionId The ID of the collection this image belongs to.
     * @param imageUrl The URL of the image.
     * @param previewImageUrl The URL of a preview image.
     * @param attribution An array of attribution strings.
     * @param attributionUrl The URL for the attribution.
     * @return A new {@link CollectionImage} object.
     */
    @CalledByNative
    static CollectionImage createImage(
            String collectionId,
            GURL imageUrl,
            GURL previewImageUrl,
            String[] attribution,
            GURL attributionUrl) {
        return new CollectionImage(
                collectionId,
                imageUrl,
                previewImageUrl,
                Arrays.asList(attribution),
                attributionUrl);
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
     * Sets the currently selected theme collection image from a theme collection.
     *
     * @param themeCollectionId The ID of the theme collection.
     * @param themeCollectionImageUrl The URL of the theme collection image.
     */
    public void setSelectedTheme(
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
    public void setCollectionTheme(CollectionImage image) {
        String attributionLine1 = image.attribution.size() > 0 ? image.attribution.get(0) : null;
        String attributionLine2 = image.attribution.size() > 1 ? image.attribution.get(1) : null;
        NtpThemeBridgeJni.get()
                .setCollectionTheme(
                        mNativeNtpThemeBridge,
                        image.collectionId,
                        image.imageUrl,
                        image.previewImageUrl,
                        attributionLine1,
                        attributionLine2,
                        image.attributionUrl);
    }

    /**
     * Callback from native code, triggered when the custom background image has been successfully
     * updated. This can occur after a new theme is selected or when a daily refresh happens.
     */
    @CalledByNative
    @VisibleForTesting
    void onCustomBackgroundImageUpdated() {
        if (mNativeNtpThemeBridge == 0) return;

        CustomBackgroundInfo info =
                NtpThemeBridgeJni.get().getCustomBackgroundInfo(mNativeNtpThemeBridge);

        if (info == null || !info.backgroundUrl.isValid() || info.backgroundUrl.isEmpty()) {
            setSelectedTheme(/* themeCollectionId= */ null, /* themeCollectionImageUrl= */ null);
            return;
        }

        setSelectedTheme(info.collectionId, info.backgroundUrl);
        mOnThemeImageSelectedCallback.run();
        mIsDailyRefreshEnabled = info.isDailyRefreshEnabled;

        // TODO: update(turn on or turn off) daily update button if the current page is that
        // particular single theme collection bottom sheet.
    }

    /** Sets the user-selected background image. */
    public void selectLocalBackgroundImage() {
        if (mNativeNtpThemeBridge == 0) return;

        NtpThemeBridgeJni.get().selectLocalBackgroundImage(mNativeNtpThemeBridge);
    }

    /** Resets the custom background. */
    public void resetCustomBackground() {
        if (mNativeNtpThemeBridge == 0) return;

        NtpThemeBridgeJni.get().resetCustomBackground(mNativeNtpThemeBridge);
    }

    /**
     * Factory method called by native code to construct a {@link CustomBackgroundInfo} object.
     *
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     */
    @CalledByNative
    private static CustomBackgroundInfo createCustomBackgroundInfo(
            GURL backgroundUrl,
            String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled) {
        return new CustomBackgroundInfo(
                backgroundUrl, collectionId, isUploadedImage, isDailyRefreshEnabled);
    }

    @NativeMethods
    public interface Natives {
        long init(Profile profile, NtpThemeBridge caller);

        void destroy(long nativeNtpThemeBridge);

        void getBackgroundCollections(long nativeNtpThemeBridge, Callback<Object[]> callback);

        void getBackgroundImages(
                long nativeNtpThemeBridge, String collectionId, Callback<Object[]> callback);

        void setCollectionTheme(
                long nativeNtpThemeBridge,
                String collectionId,
                GURL imageUrl,
                GURL previewImageUrl,
                @Nullable String attributionLine1,
                @Nullable String attributionLine2,
                GURL attributionUrl);

        @Nullable CustomBackgroundInfo getCustomBackgroundInfo(long nativeNtpThemeBridge);

        void selectLocalBackgroundImage(long nativeNtpThemeBridge);

        void resetCustomBackground(long nativeNtpThemeBridge);
    }
}
