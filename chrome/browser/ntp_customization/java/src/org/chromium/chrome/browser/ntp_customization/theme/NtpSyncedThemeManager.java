// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
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
        mNtpSyncedThemeBridge = new NtpSyncedThemeBridge(mProfile, this::onThemeCollectionSynced);
    }

    /** Cleans up the C++ side of {@link NtpSyncedThemeBridge}. */
    public void destroy() {
        if (mNtpSyncedThemeBridge != null) {
            mNtpSyncedThemeBridge.destroy();
            mNtpSyncedThemeBridge = null;
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

        if (mNtpSyncedThemeBridge != null) {
            mNtpSyncedThemeBridge.fetchNextThemeCollectionImage();
        }
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

            // TODO(crbug.com/488439751): Handle synced reset to Default theme in a follow-up.
            return;
        }

        NtpCustomizationUtils.fetchThemeCollectionImage(
                mImageFetcher,
                info.backgroundUrl,
                (bitmap) -> handleFetchedThemeCollectionImage(info, bitmap));
    }

    /**
     * Handles the downloaded theme collection bitmap image and applies or pre-fetches it based on
     * the sync and daily refresh state.
     *
     * @param info The {@link CustomBackgroundInfo} containing theme collection metadata.
     * @param bitmap The fetched background image bitmap, or {@code null} if downloading failed.
     */
    private void handleFetchedThemeCollectionImage(
            CustomBackgroundInfo info, @Nullable Bitmap bitmap) {
        if (bitmap == null) {
            return;
        }

        BackgroundImageInfo backgroundImageInfo =
                NtpCustomizationUtils.getDefaultBackgroundImageInfo(mContext, bitmap);

        boolean isSyncUpdate =
                mNtpSyncedThemeBridge != null && mNtpSyncedThemeBridge.isProcessingSyncUpdate();

        if (!isSyncUpdate) {
            // Case 1: Local device next-day daily refresh pre-fetch.
            NtpCustomizationUtils.saveDailyRefreshBackgroundInfo(info, bitmap, backgroundImageInfo);
        } else if (info.isDailyRefreshEnabled) {
            // Case 3: Synced daily refresh setup from another device.
            // TODO(crbug.com/488439751): For synced theme collection daily updates,
            // applying the fetched image on the next NTP launch, saving the
            // background info, and subsequently pre-fetching the following day's
            // image will be implemented in a follow-up.
        } else {
            // Case 2: Synced static theme collection image from another device.
            String fileId = NtpCustomizationUtils.getFileName(info.backgroundUrl.getPath());
            NtpBackgroundDataThemeCollection themeCollectionData =
                    new NtpBackgroundDataThemeCollection(
                            PlatformType.ANDROID,
                            info,
                            backgroundImageInfo,
                            bitmap,
                            /* primaryColor= */ null,
                            /* fileIdHash= */ fileId);
            NtpCustomizationConfigManager.getInstance()
                    .onSyncedThemeCollectionImageChanged(themeCollectionData);
        }
    }
}
