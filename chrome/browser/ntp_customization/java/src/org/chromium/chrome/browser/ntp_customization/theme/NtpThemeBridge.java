// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CollectionImage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** The JNI bridge that deal with theme collections for the NTP. */
@NullMarked
public class NtpThemeBridge {

    private long mNativeNtpThemeBridge;

    // The theme collection that the user has currently chosen.
    private @Nullable String mSelectedThemeCollectionId;
    // The theme collection image that the user has currently chosen.
    private @Nullable GURL mSelectedThemeCollectionImageUrl;
    private final ObserverList<ThemeCollectionSelectionListener> mThemeCollectionSelectionListeners;

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
     */
    public NtpThemeBridge(Profile profile) {
        mNativeNtpThemeBridge = NtpThemeBridgeJni.get().init(profile);
        mThemeCollectionSelectionListeners = new ObserverList<>();
        // TODO(crbug.com/423579377): Load selected theme collection information from
        // SharedPreferences here
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

    @NativeMethods
    public interface Natives {
        long init(Profile profile);

        void destroy(long nativeNtpThemeBridge);

        void getBackgroundCollections(long nativeNtpThemeBridge, Callback<Object[]> callback);

        void getBackgroundImages(
                long nativeNtpThemeBridge, String collectionId, Callback<Object[]> callback);
    }
}
