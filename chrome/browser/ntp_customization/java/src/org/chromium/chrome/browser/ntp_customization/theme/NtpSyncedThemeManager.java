// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;

/** Manages the lifecycle of NtpSyncedThemeBridge. */
@NullMarked
public class NtpSyncedThemeManager {
    private final NtpSyncedThemeBridge mNtpSyncedThemeBridge;
    private final Context mContext;
    private final @Nullable ImageFetcher mImageFetcher;

    /**
     * Constructs a new NtpSyncedThemeManager.
     *
     * @param context The application context.
     * @param profile The profile for which the {@link NtpSyncedThemeBridge} is created.
     */
    public NtpSyncedThemeManager(Context context, Profile profile) {
        mContext = context;
        mImageFetcher = NtpCustomizationUtils.createImageFetcher(profile);
        mNtpSyncedThemeBridge = new NtpSyncedThemeBridge(profile, this::onThemeCollectionSynced);
    }

    /** Cleans up the C++ side of {@link NtpSyncedThemeBridge}. */
    public void destroy() {
        mNtpSyncedThemeBridge.destroy();
    }

    /**
     * Called when the synced theme collection has been updated.
     *
     * @param info The {@link CustomBackgroundInfo} containing custom background info.
     */
    private void onThemeCollectionSynced(@Nullable CustomBackgroundInfo info) {
        if (info == null
                || !info.backgroundUrl.isValid()
                || info.backgroundUrl.isEmpty()
                || mImageFetcher == null) {
            return;
        }

        NtpCustomizationUtils.fetchThemeCollectionImage(
                mImageFetcher,
                info.backgroundUrl,
                (bitmap) -> {
                    if (bitmap != null) {
                        BackgroundImageInfo backgroundImageInfo =
                                NtpCustomizationUtils.calculateInitialThemeCollectionImageMatrices(
                                        mContext, bitmap);
                        NtpCustomizationUtils.saveBackgroundInfoForThemeCollectionOrUploadedImage(
                                info, bitmap, backgroundImageInfo);
                    }
                });
    }
}
