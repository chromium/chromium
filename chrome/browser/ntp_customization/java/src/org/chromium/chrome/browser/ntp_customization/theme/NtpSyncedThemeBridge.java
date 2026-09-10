// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
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
        long init(Profile profile, NtpSyncedThemeBridge caller);

        void destroy(long nativeNtpSyncedThemeBridge);

        void fetchNextThemeCollectionImage(long nativeNtpSyncedThemeBridge);

        @Nullable CustomBackgroundInfo getCustomBackgroundInfo(long nativeNtpSyncedThemeBridge);

        boolean isProcessingSyncUpdate(long nativeNtpSyncedThemeBridge);
    }
}
