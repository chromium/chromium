// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Intent;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.media.MediaSessionHelper;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * A tab helper that wraps {@link MediaSessionHelper} and is responsible for Chrome-specific
 * behavior.
 */
@NullMarked
public class MediaSessionTabHelper implements MediaSessionHelper.Delegate, UserData {
    private static final Class<MediaSessionTabHelper> USER_DATA_KEY = MediaSessionTabHelper.class;

    private @Nullable Tab mTab;
    @VisibleForTesting @Nullable MediaSessionHelper mMediaSessionHelper;

    @VisibleForTesting
    final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onContentChanged(Tab tab) {
                    assert tab == mTab;
                    maybeCreateOrUpdateMediaSessionHelper();
                }

                @Override
                public void onFaviconUpdated(
                        Tab tab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
                    assert tab == mTab;

                    if (mMediaSessionHelper == null) return;

                    mMediaSessionHelper.updateFavicon(icon);
                }

                @Override
                public void onDestroyed(Tab tab) {
                    assert mTab == tab;

                    if (mMediaSessionHelper != null) mMediaSessionHelper.destroy();
                    mTab.removeObserver(this);
                    mTab = null;
                }

                @Override
                public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                    // Intentionally do nothing to prevent automatic observer removal on detachment.
                }
            };

    @VisibleForTesting
    MediaSessionTabHelper(Tab tab) {
        mTab = tab;
        mTab.addObserver(mTabObserver);
        maybeCreateOrUpdateMediaSessionHelper();
    }

    private void maybeCreateOrUpdateMediaSessionHelper() {
        if (mTab == null) return;
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) {
            if (mMediaSessionHelper != null) {
                mMediaSessionHelper.destroy();
                mMediaSessionHelper = null;
            }
        } else {
            if (mMediaSessionHelper != null) {
                mMediaSessionHelper.setWebContents(webContents);
            } else {
                mMediaSessionHelper = new MediaSessionHelper(webContents, this);
            }
        }
    }

    /**
     * Retrieves the {@link MediaSessionTabHelper} for the given {@link Tab}, creating it if it
     * doesn't already exist.
     *
     * @param tab The Tab to get the helper for.
     * @return The {@link MediaSessionTabHelper}, or null if UserDataHost is null.
     */
    public static @Nullable MediaSessionTabHelper from(Tab tab) {
        if (tab.getUserDataHost() == null || tab.getWebContents() == null) return null;
        MediaSessionTabHelper helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) {
            helper =
                    tab.getUserDataHost()
                            .setUserData(USER_DATA_KEY, new MediaSessionTabHelper(tab));
        }
        return helper;
    }

    @Override
    public Intent createBringTabToFrontIntent() {
        return IntentHandler.createTrustedBringTabToFrontIntent(
                assumeNonNull(mTab).getId(), IntentHandler.BringToFrontSource.NOTIFICATION);
    }

    @Override
    public LargeIconBridge getLargeIconBridge() {
        return new LargeIconBridge(assumeNonNull(mTab).getProfile());
    }

    @Override
    public MediaNotificationInfo.Builder createMediaNotificationInfoBuilder() {
        return new MediaNotificationInfo.Builder()
                .setInstanceId(assumeNonNull(mTab).getId())
                .setId(R.id.media_playback_notification);
    }

    @Override
    public void showMediaNotification(MediaNotificationInfo notificationInfo) {
        ChromeMediaNotificationManager.show(notificationInfo);
    }

    @Override
    public void hideMediaNotification() {
        if (mTab == null) return; // Return early if onDestroy was already called.

        MediaNotificationManager.hide(mTab.getId(), R.id.media_playback_notification);
    }

    @Override
    public void activateAndroidMediaSession() {
        if (mTab == null) return; // Return early if onDestroy was already called.

        MediaNotificationManager.activateAndroidMediaSession(
                mTab.getId(), R.id.media_playback_notification);
    }
}
