// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * The JNI bridge that deal with theme collections for the NTP. This object exists only if a NTP
 * customization bottom sheet is visible.
 */
@NullMarked
public class NtpThemeCollectionBridge {
    private final Callback<@Nullable CustomBackgroundInfo> mOnCustomBackgroundImageUpdatedCallback;
    private long mNativeNtpThemeCollectionBridge;

    /**
     * Constructs a new NtpThemeCollectionBridge.
     *
     * @param profile The profile for which this bridge is created.
     * @param onCustomBackgroundImageUpdatedCallback The callback to run when the theme collection
     *     is updated.
     */
    public NtpThemeCollectionBridge(
            Profile profile,
            Callback<@Nullable CustomBackgroundInfo> onCustomBackgroundImageUpdatedCallback) {
        mNativeNtpThemeCollectionBridge = NtpThemeCollectionBridgeJni.get().init(profile, this);
        mOnCustomBackgroundImageUpdatedCallback = onCustomBackgroundImageUpdatedCallback;
    }

    /** Cleans up the C++ side of this class. */
    public void destroy() {
        assert mNativeNtpThemeCollectionBridge != 0;
        NtpThemeCollectionBridgeJni.get().destroy(mNativeNtpThemeCollectionBridge);
        mNativeNtpThemeCollectionBridge = 0;
    }

    /**
     * Fetches the list of background collections.
     *
     * @param callback The callback to be invoked with the list of collections.
     */
    public void getBackgroundCollections(Callback<@Nullable List<BackgroundCollection>> callback) {
        NtpThemeCollectionBridgeJni.get()
                .getBackgroundCollections(
                        mNativeNtpThemeCollectionBridge,
                        new Callback<Object[]>() {
                            @Override
                            public void onResult(Object[] collections) {
                                if (collections == null) {
                                    callback.onResult(null);
                                    return;
                                }
                                List<BackgroundCollection> collectionList = new ArrayList<>();
                                for (Object o : collections) {
                                    if (o instanceof BackgroundCollection collection) {
                                        collectionList.add(collection);
                                    } else {
                                        throw new AssertionError(
                                                "Collections contain object which is not"
                                                        + " BackgroundCollection");
                                    }
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
        NtpThemeCollectionBridgeJni.get()
                .getBackgroundImages(
                        mNativeNtpThemeCollectionBridge,
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
                                    if (o instanceof CollectionImage image) {
                                        imageList.add(image);
                                    } else {
                                        throw new AssertionError(
                                                "Images contain object which is not"
                                                        + " CollectionImage");
                                    }
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
     * @param hash The hash of collection ID.
     * @return A new {@link BackgroundCollection} object.
     */
    @CalledByNative
    static BackgroundCollection createCollection(
            String id, String label, GURL previewImageUrl, int hash) {
        return new BackgroundCollection(id, label, previewImageUrl, hash);
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

    /**
     * Sets the New Tab Page background to a specific image from a theme collection.
     *
     * @param image The {@link CollectionImage} selected by the user.
     */
    public void setThemeCollectionImage(CollectionImage image) {
        String attributionLine1 = image.attribution.size() > 0 ? image.attribution.get(0) : null;
        String attributionLine2 = image.attribution.size() > 1 ? image.attribution.get(1) : null;
        NtpThemeCollectionBridgeJni.get()
                .setThemeCollectionImage(
                        mNativeNtpThemeCollectionBridge,
                        image.collectionId,
                        image.imageUrl,
                        image.previewImageUrl,
                        attributionLine1,
                        attributionLine2,
                        image.attributionUrl);
    }

    /**
     * Sets the New Tab Page background to a theme collection with daily refresh function enabled.
     *
     * @param themeCollectionId The id of the theme collection
     */
    public void setThemeCollectionDailyRefreshed(String themeCollectionId) {
        if (mNativeNtpThemeCollectionBridge == 0) return;

        NtpThemeCollectionBridgeJni.get()
                .setThemeCollectionDailyRefreshed(
                        mNativeNtpThemeCollectionBridge, themeCollectionId);
    }

    /**
     * Callback from native code, triggered when the custom background image has been successfully
     * updated. This can occur after a new theme is selected.
     */
    @CalledByNative
    @VisibleForTesting
    public void onCustomBackgroundImageUpdated() {
        if (mNativeNtpThemeCollectionBridge == 0) {
            return;
        }

        CustomBackgroundInfo customBackgroundInfo =
                NtpThemeCollectionBridgeJni.get()
                        .getCustomBackgroundInfo(mNativeNtpThemeCollectionBridge);
        mOnCustomBackgroundImageUpdatedCallback.onResult(customBackgroundInfo);
    }

    /** Sets the user-selected background image. */
    public void selectLocalBackgroundImage() {
        if (mNativeNtpThemeCollectionBridge == 0) return;

        NtpThemeCollectionBridgeJni.get()
                .selectLocalBackgroundImage(mNativeNtpThemeCollectionBridge);
    }

    /** Resets the custom background. */
    public void resetCustomBackground() {
        if (mNativeNtpThemeCollectionBridge == 0) return;

        NtpThemeCollectionBridgeJni.get().resetCustomBackground(mNativeNtpThemeCollectionBridge);
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
        long init(Profile profile, NtpThemeCollectionBridge caller);

        void destroy(long nativeNtpThemeCollectionBridge);

        void getBackgroundCollections(
                long nativeNtpThemeCollectionBridge, Callback<Object[]> callback);

        void getBackgroundImages(
                long nativeNtpThemeCollectionBridge,
                String collectionId,
                Callback<Object[]> callback);

        void setThemeCollectionImage(
                long nativeNtpThemeCollectionBridge,
                String collectionId,
                GURL imageUrl,
                GURL previewImageUrl,
                @Nullable String attributionLine1,
                @Nullable String attributionLine2,
                GURL attributionUrl);

        void setThemeCollectionDailyRefreshed(
                long nativeNtpThemeCollectionBridge, String collectionId);

        @Nullable CustomBackgroundInfo getCustomBackgroundInfo(long nativeNtpThemeCollectionBridge);

        void selectLocalBackgroundImage(long nativeNtpThemeCollectionBridge);

        void resetCustomBackground(long nativeNtpThemeCollectionBridge);
    }
}
