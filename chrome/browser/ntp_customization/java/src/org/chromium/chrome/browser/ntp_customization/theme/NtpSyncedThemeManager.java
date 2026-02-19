// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

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
    private final Context mContext;
    private final Profile mProfile;
    private final @Nullable ImageFetcher mImageFetcher;
    private @Nullable NtpSyncedThemeBridge mNtpSyncedThemeBridge;

    /**
     * Constructs a new NtpSyncedThemeManager.
     *
     * @param context The application context.
     * @param profile The profile for which the {@link NtpSyncedThemeBridge} is created.
     */
    public NtpSyncedThemeManager(Context context, Profile profile) {
        mContext = context;
        mProfile = profile;
        mImageFetcher = NtpCustomizationUtils.createImageFetcher(profile);
    }

    /** Cleans up the C++ side of {@link NtpSyncedThemeBridge}. */
    public void destroy() {
        if (mNtpSyncedThemeBridge != null) {
            mNtpSyncedThemeBridge.destroy();
        }
    }

    /**
     * Called after a daily refresh for a theme collection is applied. This triggers fetching the
     * image for the next day's refresh if one hasn't been fetched already.
     */
    public void fetchNextThemeCollectionImageAfterDailyRefreshApplied() {
        if (NtpCustomizationUtils.getNtpBackgroundType() != THEME_COLLECTION) {
            return;
        }

        CustomBackgroundInfo customBackgroundInfo =
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference();
        if (customBackgroundInfo == null || !customBackgroundInfo.isDailyRefreshEnabled) {
            return;
        }

        if (NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference()
                != null) {
            return;
        }

        // TODO(crbug.com/423579377): Move this back to constructor when adding specific android
        // service for theme collections.
        mNtpSyncedThemeBridge = new NtpSyncedThemeBridge(mProfile, this::onThemeCollectionSynced);
        mNtpSyncedThemeBridge.fetchNextThemeCollectionImage();
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
                                NtpCustomizationUtils.getDefaultBackgroundImageInfo(
                                        mContext, bitmap);
                        NtpCustomizationUtils.saveDailyRefreshBackgroundInfo(
                                info, bitmap, backgroundImageInfo);
                        destroy();
                    }
                });
    }
}
