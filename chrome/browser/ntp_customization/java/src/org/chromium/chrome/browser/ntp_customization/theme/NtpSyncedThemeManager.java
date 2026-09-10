// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;

/** Manages the lifecycle of NtpSyncedThemeBridge. */
@NullMarked
public class NtpSyncedThemeManager implements NtpSyncedThemeBridge.Observer {
    private final Context mContext;
    private final Profile mProfile;
    private final NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private final @Nullable ImageFetcher mImageFetcher;
    private @Nullable NtpSyncedThemeBridge mNtpSyncedThemeBridge;
    // Tracks the URL of the most recently requested synced background image to prevent race
    // conditions (e.g. if a newer Chrome color or default reset arrives while the image download is
    // in-flight, or if multiple sync updates arrive in rapid succession).
    // TODO(crbug.com/488439751): Handle when a user manually selects an image while a synced image
    // download is in-flight, so the in-flight download does not overwrite the user's manual
    // selection when completed.
    private @Nullable String mLatestSyncedBackgroundUrl;

    /**
     * Constructs a new NtpSyncedThemeManager.
     *
     * @param context The application context.
     * @param profile The profile for which the {@link NtpSyncedThemeBridge} is created.
     */
    public NtpSyncedThemeManager(Context context, Profile profile) {
        this(context, profile, NtpCustomizationConfigManager.getInstance());
    }

    @VisibleForTesting
    NtpSyncedThemeManager(
            Context context,
            Profile profile,
            NtpCustomizationConfigManager ntpCustomizationConfigManager) {
        mContext = context.getApplicationContext();
        mProfile = profile;
        mNtpCustomizationConfigManager = ntpCustomizationConfigManager;
        mImageFetcher = NtpCustomizationUtils.createImageFetcher(profile);
        mNtpSyncedThemeBridge = new NtpSyncedThemeBridge(mProfile, this);
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
    @Override
    public void onThemeCollectionSynced(@Nullable CustomBackgroundInfo info) {
        if (info == null
                || !info.backgroundUrl.isValid()
                || info.backgroundUrl.isEmpty()
                || mImageFetcher == null) {
            return;
        }

        mLatestSyncedBackgroundUrl = info.backgroundUrl.getSpec();
        NtpCustomizationUtils.fetchThemeCollectionImage(
                mImageFetcher,
                info.backgroundUrl,
                (bitmap) -> handleFetchedThemeCollectionImage(info, bitmap));
    }

    /**
     * Called when a Chrome color has arrived from sync.
     *
     * @param colorId The synced color ID.
     */
    @Override
    public void onChromeColorSynced(int colorId) {
        if (colorId <= NtpThemeColorId.DEFAULT || colorId >= NtpThemeColorId.NUM_ENTRIES) {
            onDefaultThemeSynced();
            return;
        }

        mLatestSyncedBackgroundUrl = null;

        NtpBackgroundDataColor colorData =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        colorId,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpCustomizationConfigManager.onSyncedChromeColorChanged(mContext, colorData);
    }

    /** Called when the theme is reset to default from sync. */
    @Override
    public void onDefaultThemeSynced() {
        mLatestSyncedBackgroundUrl = null;
        mNtpCustomizationConfigManager.onSyncedDefaultThemeReset(mContext);
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

        if (!info.backgroundUrl.getSpec().equals(mLatestSyncedBackgroundUrl)) {
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
            @ColorInt Integer primaryColor = NtpCustomizationUtils.getContentBasedSeedColor(bitmap);
            NtpBackgroundDataThemeCollection themeCollectionData =
                    new NtpBackgroundDataThemeCollection(
                            PlatformType.ANDROID,
                            info,
                            backgroundImageInfo,
                            bitmap,
                            primaryColor,
                            /* fileIdHash= */ fileId);
            mNtpCustomizationConfigManager.onSyncedThemeCollectionImageChanged(
                    mContext, themeCollectionData);
        }
    }
}
