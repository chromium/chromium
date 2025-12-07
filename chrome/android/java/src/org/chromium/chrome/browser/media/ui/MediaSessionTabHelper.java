// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Intent;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

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
public class MediaSessionTabHelper implements MediaSessionHelper.Delegate {
    private @Nullable Tab mTab;
    @VisibleForTesting MediaSessionHelper mMediaSessionHelper;

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
        WebContents webContents = assumeNonNull(assumeNonNull(mTab).getWebContents());
        if (mMediaSessionHelper != null) {
            mMediaSessionHelper.setWebContents(webContents);
        } else if (mTab.getWebContents() != null) {
            mMediaSessionHelper = new MediaSessionHelper(webContents, this);
        }
    }

    /**
     * Creates the {@link MediaSessionTabHelper} for the given {@link Tab}.
     * @param tab the tab to attach the helper to.
     */
    public static void createForTab(Tab tab) {
        new MediaSessionTabHelper(tab);
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
