// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
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
    private final Callback<@Nullable CustomBackgroundInfo> mOnThemeCollectionSyncedCallback;
    private long mNativeNtpSyncedThemeBridge;

    /**
     * Constructs a new NtpSyncedThemeBridge.
     *
     * @param profile The profile for which this bridge is created.
     * @param onThemeCollectionSyncedCallback The callback to be invoked when the theme collection
     *     is synced or daily updated.
     */
    public NtpSyncedThemeBridge(
            Profile profile,
            Callback<@Nullable CustomBackgroundInfo> onThemeCollectionSyncedCallback) {
        mNativeNtpSyncedThemeBridge = NtpSyncedThemeBridgeJni.get().init(profile, this);
        mOnThemeCollectionSyncedCallback = onThemeCollectionSyncedCallback;
    }

    /** Cleans up the C++ side of this class. */
    public void destroy() {
        assert mNativeNtpSyncedThemeBridge != 0;
        NtpSyncedThemeBridgeJni.get().destroy(mNativeNtpSyncedThemeBridge);
        mNativeNtpSyncedThemeBridge = 0;
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
        mOnThemeCollectionSyncedCallback.onResult(info);
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

        @Nullable CustomBackgroundInfo getCustomBackgroundInfo(long nativeNtpSyncedThemeBridge);
    }
}
