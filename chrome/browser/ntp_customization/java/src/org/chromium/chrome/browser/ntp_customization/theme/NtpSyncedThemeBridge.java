// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/**
 * The JNI bridge that deal with theme collections for the NTP. This class is used to store the most
 * up-to-date theme collection information, which can originate from the current manual settings, a
 * daily update, or a profile sync. The stored value will be used when Chrome is relaunched.
 */
@NullMarked
public class NtpSyncedThemeBridge {
    /** Observer interface for synced theme updates. */
    public interface Observer {
        /** Dispatched when a theme collection background arrives from sync or daily refresh. */
        void onThemeCollectionSynced(@Nullable CustomBackgroundInfo info);

        /** Dispatched when a Chrome color arrives from sync. */
        void onChromeColorSynced(int colorId);

        /** Dispatched when the theme is reset to default from sync. */
        void onDefaultThemeSynced();
    }

    private final Observer mObserver;
    private long mNativeNtpSyncedThemeBridge;

    /**
     * Constructs a new NtpSyncedThemeBridge with an Observer.
     *
     * @param profile The profile for which this bridge is created.
     * @param observer The observer to receive synced theme changes.
     */
    public NtpSyncedThemeBridge(Profile profile, Observer observer) {
        // Set the observer before calling native init(), since init attaches this bridge to
        // NtpAndroidCustomBackgroundService, which may immediately notify this bridge of an
        // already-existing synced background.
        mObserver = observer;
        mNativeNtpSyncedThemeBridge = NtpSyncedThemeBridgeJni.get().init(profile, this);
    }

    /** Cleans up the C++ side of this class. */
    public void destroy() {
        if (mNativeNtpSyncedThemeBridge != 0) {
            NtpSyncedThemeBridgeJni.get().destroy(mNativeNtpSyncedThemeBridge);
            mNativeNtpSyncedThemeBridge = 0;
        }
    }

    /** Fetches the next image for a theme collection with daily refresh enabled. */
    public void fetchNextThemeCollectionImage() {
        if (mNativeNtpSyncedThemeBridge == 0) return;

        NtpSyncedThemeBridgeJni.get().fetchNextThemeCollectionImage(mNativeNtpSyncedThemeBridge);
    }

    /**
     * Sets the New Tab Page theme to a specific Chrome color and notifies the sync bridge.
     *
     * @param colorId The ID of the Chrome color.
     */
    public void setChromeColor(int colorId) {
        if (mNativeNtpSyncedThemeBridge == 0) return;

        NtpSyncedThemeBridgeJni.get().setChromeColor(mNativeNtpSyncedThemeBridge, colorId);
    }

    /** Resets the New Tab Page theme to default and notifies the sync bridge. */
    public void resetCustomBackgroundInfo() {
        if (mNativeNtpSyncedThemeBridge == 0) return;

        NtpSyncedThemeBridgeJni.get().resetCustomBackgroundInfo(mNativeNtpSyncedThemeBridge);
    }

    /** Sets the user-uploaded background image and marks it local to the device. */
    public void selectLocalBackgroundImage() {
        if (mNativeNtpSyncedThemeBridge == 0) return;

        NtpSyncedThemeBridgeJni.get().selectLocalBackgroundImage(mNativeNtpSyncedThemeBridge);
    }

    /**
     * Updates the theme collection background with collection ID, primary color, and daily refresh
     * state, and notifies the sync bridge.
     *
     * @param backgroundUrl The URL of the background image.
     * @param collectionId The ID of the theme collection.
     * @param attribution The attribution of the background image.
     * @param primaryColor The primary color extracted from the theme collection image.
     * @param isDailyRefresh Whether daily refresh is enabled for this theme.
     */
    public void updateCustomBackgroundPrefsWithColor(
            GURL backgroundUrl,
            String collectionId,
            @Nullable String attribution,
            @Nullable @ColorInt Integer primaryColor,
            boolean isDailyRefresh) {
        if (mNativeNtpSyncedThemeBridge == 0) return;

        int color = primaryColor != null ? primaryColor : 0;
        NtpSyncedThemeBridgeJni.get()
                .updateCustomBackgroundPrefsWithColor(
                        mNativeNtpSyncedThemeBridge,
                        backgroundUrl,
                        collectionId,
                        attribution,
                        color,
                        isDailyRefresh);
    }

    /** Exposes whether the C++ service is actively processing a sync update. */
    public boolean isProcessingSyncUpdate() {
        if (mNativeNtpSyncedThemeBridge == 0) return false;
        return NtpSyncedThemeBridgeJni.get().isProcessingSyncUpdate(mNativeNtpSyncedThemeBridge);
    }

    /**
     * Callback from native code, triggered when the custom background image has been successfully
     * updated. This can occur after a new theme is selected or when a daily refresh happens.
     */
    @CalledByNative
    @VisibleForTesting
    void onCustomBackgroundImageUpdated() {
        if (mNativeNtpSyncedThemeBridge == 0) {
            return;
        }

        CustomBackgroundInfo info =
                NtpSyncedThemeBridgeJni.get().getCustomBackgroundInfo(mNativeNtpSyncedThemeBridge);
        mObserver.onThemeCollectionSynced(info);
    }

    /**
     * Called by native code when a Chrome color theme has been received from Chrome Sync.
     * Dispatches the event to the registered observer.
     *
     * @param colorId The synced Chrome color ID.
     */
    @CalledByNative
    @VisibleForTesting
    void onChromeColorSynced(int colorId) {
        mObserver.onChromeColorSynced(colorId);
    }

    /**
     * Called by native code when the NTP theme has been reset to default from Chrome Sync.
     * Dispatches the event to the registered observer.
     */
    @CalledByNative
    @VisibleForTesting
    void onDefaultThemeSynced() {
        mObserver.onDefaultThemeSynced();
    }

    /**
     * Factory method called by native code to construct a {@link CustomBackgroundInfo} object.
     *
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     * @param attribution The attribution string of the background image.
     */
    @CalledByNative
    @VisibleForTesting
    static CustomBackgroundInfo createCustomBackgroundInfo(
            @JniType("GURL") GURL backgroundUrl,
            @JniType("std::string") String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled,
            @JniType("std::string") String attribution) {
        return new CustomBackgroundInfo(
                backgroundUrl, collectionId, isUploadedImage, isDailyRefreshEnabled, attribution);
    }

    @NativeMethods
    public interface Natives {
        long init(Profile profile, NtpSyncedThemeBridge caller);

        void destroy(long nativeNtpSyncedThemeBridge);

        void fetchNextThemeCollectionImage(long nativeNtpSyncedThemeBridge);

        @Nullable CustomBackgroundInfo getCustomBackgroundInfo(long nativeNtpSyncedThemeBridge);

        boolean isProcessingSyncUpdate(long nativeNtpSyncedThemeBridge);

        void setChromeColor(long nativeNtpSyncedThemeBridge, int colorId);

        void resetCustomBackgroundInfo(long nativeNtpSyncedThemeBridge);

        void selectLocalBackgroundImage(long nativeNtpSyncedThemeBridge);

        void updateCustomBackgroundPrefsWithColor(
                long nativeNtpSyncedThemeBridge,
                @JniType("GURL") GURL backgroundUrl,
                @JniType("std::string") String collectionId,
                @JniType("std::string") @Nullable String attribution,
                int primaryColor,
                boolean isDailyRefresh);
    }
}
