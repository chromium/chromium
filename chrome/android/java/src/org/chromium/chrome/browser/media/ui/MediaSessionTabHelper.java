// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.document.ChromeIntentUtil;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.metrics.MediaNotificationUma;
import org.chromium.chrome.browser.metrics.MediaSessionUMA;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.media_session.mojom.MediaSessionAction;
import org.chromium.net.GURLUtils;
import org.chromium.services.media_session.MediaImage;
import org.chromium.services.media_session.MediaMetadata;
import org.chromium.services.media_session.MediaPosition;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;
import java.util.Set;

/**
 * A tab helper responsible for enabling/disabling media controls and passing
 * media actions from the controls to the {@link org.chromium.content.browser.MediaSession}
 */
public class MediaSessionTabHelper implements MediaImageCallback {
    private static final String TAG = "MediaSession";

    private static final String UNICODE_PLAY_CHARACTER = "\u25B6";
    @VisibleForTesting
    static final int HIDE_NOTIFICATION_DELAY_MILLIS = 1000;

    private Tab mTab;
    @VisibleForTesting
    LargeIconBridge mLargeIconBridge;
    private Bitmap mPageMediaImage;
    @VisibleForTesting
    Bitmap mFavicon;
    // Set to true if favicon update callback was called at least once for the current tab.
    private boolean mMaybeHasFavicon;
    private Bitmap mCurrentMediaImage;
    private String mOrigin;
    @VisibleForTesting
    MediaSessionObserver mMediaSessionObserver;
    private int mPreviousVolumeControlStream = AudioManager.USE_DEFAULT_STREAM_TYPE;
    private MediaNotificationInfo.Builder mNotificationInfoBuilder;
    // The fallback title if |mPageMetadata| is null or its title is empty.
    private String mFallbackTitle;
    // The metadata set by the page.
    private MediaMetadata mPageMetadata;
    // The currently showing metadata.
    private MediaMetadata mCurrentMetadata;
    private MediaImageManager mMediaImageManager;
    private Set<Integer> mMediaSessionActions;
    private @Nullable MediaPosition mMediaPosition;
    private Handler mHandler;
    // The delayed task to hide notification. Hiding notification can be immediate or delayed.
    // Delayed hiding will schedule this delayed task to |mHandler|. The task will be canceled when
    // showing or immediate hiding.
    private Runnable mHideNotificationDelayedTask;

    // Used to override the MediaSession object get from WebContents. This is to work around the
    // static getter {@link MediaSession#fromWebContents()}.
    @VisibleForTesting
    static MediaSession sOverriddenMediaSession;

    @VisibleForTesting
    @Nullable
    MediaSessionObserver getMediaSessionObserverForTesting() {
        return mMediaSessionObserver;
    }

    private MediaNotificationListener mControlsListener = new MediaNotificationListener() {
        @Override
        public void onPlay(int actionSource) {
            if (isNotificationHiddingOrHidden()) return;

            MediaSessionUMA.recordPlay(
                    MediaSessionTabHelper.convertMediaActionSourceToUMA(actionSource));

            if (mMediaSessionObserver.getMediaSession() == null) return;

            mMediaSessionObserver.getMediaSession().resume();
        }

        @Override
        public void onPause(int actionSource) {
            if (isNotificationHiddingOrHidden()) return;

            MediaSessionUMA.recordPause(
                    MediaSessionTabHelper.convertMediaActionSourceToUMA(actionSource));

            if (mMediaSessionObserver.getMediaSession() == null) return;

            mMediaSessionObserver.getMediaSession().suspend();
        }

        @Override
        public void onStop(int actionSource) {
            if (isNotificationHiddingOrHidden()) return;

            MediaSessionUMA.recordStop(
                    MediaSessionTabHelper.convertMediaActionSourceToUMA(actionSource));

            if (mMediaSessionObserver.getMediaSession() != null) {
                mMediaSessionObserver.getMediaSession().stop();
            }
        }

        @Override
        public void onMediaSessionAction(int action) {
            if (!MediaSessionAction.isKnownValue(action)) return;
            if (mMediaSessionObserver != null) {
                mMediaSessionObserver.getMediaSession().didReceiveAction(action);
            }
        }

        @Override
        public void onMediaSessionSeekTo(long pos) {
            if (mMediaSessionObserver == null) return;
            mMediaSessionObserver.getMediaSession().seekTo(pos);
        }
    };

    private void hideNotificationDelayed() {
        if (mTab == null) return;
        if (mHideNotificationDelayedTask != null) return;

        mHideNotificationDelayedTask = new Runnable() {
            @Override
            public void run() {
                mHideNotificationDelayedTask = null;
                hideNotificationInternal();
            }
        };
        mHandler.postDelayed(mHideNotificationDelayedTask, HIDE_NOTIFICATION_DELAY_MILLIS);

        mNotificationInfoBuilder = null;
        mFavicon = null;
    }

    private void hideNotificationImmediately() {
        if (mTab == null) return;
        if (mHideNotificationDelayedTask != null) {
            mHandler.removeCallbacks(mHideNotificationDelayedTask);
            mHideNotificationDelayedTask = null;
        }

        hideNotificationInternal();
        mNotificationInfoBuilder = null;
    }

    /**
     * This method performs the common steps for hiding the notification. It should only be called
     * by {@link #hideNotificationDelayed()} and {@link #hideNotificationImmediately()}.
     */
    private void hideNotificationInternal() {
        MediaNotificationManager.hide(mTab.getId(), R.id.media_playback_notification);
        Activity activity = getActivityFromTab(mTab);
        if (activity != null) {
            activity.setVolumeControlStream(mPreviousVolumeControlStream);
        }
    }

    private void showNotification() {
        assert mNotificationInfoBuilder != null;
        if (mHideNotificationDelayedTask != null) {
            mHandler.removeCallbacks(mHideNotificationDelayedTask);
            mHideNotificationDelayedTask = null;
        }
        MediaNotificationManager.show(mNotificationInfoBuilder.build());
    }

    private MediaSessionObserver createMediaSessionObserver(MediaSession mediaSession) {
        return new MediaSessionObserver(mediaSession) {
            @Override
            public void mediaSessionDestroyed() {
                hideNotificationImmediately();
                cleanupMediaSessionObserver();
            }

            @Override
            public void mediaSessionStateChanged(boolean isControllable, boolean isPaused) {
                if (!isControllable) {
                    hideNotificationDelayed();
                    return;
                }

                Intent contentIntent = ChromeIntentUtil.createBringTabToFrontIntent(mTab.getId());
                if (contentIntent != null) {
                    contentIntent.putExtra(MediaNotificationUma.INTENT_EXTRA_NAME,
                            MediaNotificationUma.Source.MEDIA);
                }

                if (mFallbackTitle == null) mFallbackTitle = sanitizeMediaTitle(mTab.getTitle());

                mCurrentMetadata = getMetadata();
                mCurrentMediaImage = getCachedNotificationImage();
                mNotificationInfoBuilder =
                        new MediaNotificationInfo.Builder()
                                .setMetadata(mCurrentMetadata)
                                .setPaused(isPaused)
                                .setOrigin(mOrigin)
                                .setTabId(mTab.getId())
                                .setPrivate(mTab.isIncognito())
                                .setNotificationSmallIcon(R.drawable.audio_playing)
                                .setNotificationLargeIcon(mCurrentMediaImage)
                                .setMediaSessionImage(mPageMediaImage)
                                .setActions(MediaNotificationInfo.ACTION_PLAY_PAUSE
                                        | MediaNotificationInfo.ACTION_SWIPEAWAY)
                                .setContentIntent(contentIntent)
                                .setId(R.id.media_playback_notification)
                                .setListener(mControlsListener)
                                .setMediaSessionActions(mMediaSessionActions)
                                .setMediaPosition(mMediaPosition);

                // Do not show notification icon till we get the favicon from the LargeIconBridge
                // since we do not need to show default icon then change it to favicon. It is ok to
                // wait here since the favicon is loaded from local cache in favicon service sql
                // database.
                // Incognito Tabs need the default icon as they don't show the media icon.
                if (mTab.isIncognito() || (mCurrentMediaImage == null && !fetchFaviconImage())) {
                    mNotificationInfoBuilder.setDefaultNotificationLargeIcon(
                            R.drawable.audio_playing_square);
                }
                showNotification();
                Activity activity = getActivityFromTab(mTab);
                if (activity != null) {
                    activity.setVolumeControlStream(AudioManager.STREAM_MUSIC);
                }
            }

            @Override
            public void mediaSessionMetadataChanged(MediaMetadata metadata) {
                mPageMetadata = metadata;
                updateNotificationMetadata();
            }

            @Override
            public void mediaSessionActionsChanged(Set<Integer> actions) {
                mMediaSessionActions = actions;
                updateNotificationActions();
            }

            @Override
            public void mediaSessionArtworkChanged(List<MediaImage> images) {
                mMediaImageManager.downloadImage(images, MediaSessionTabHelper.this);
                updateNotificationMetadata();
            }

            @Override
            public void mediaSessionPositionChanged(@Nullable MediaPosition position) {
                mMediaPosition = position;
                updateNotificationPosition();
            }
        };
    }

    private void setWebContents(WebContents webContents) {
        MediaSession mediaSession = getMediaSession(webContents);
        if (mMediaSessionObserver != null
                && mediaSession == mMediaSessionObserver.getMediaSession()) {
            return;
        }

        cleanupMediaSessionObserver();
        mMediaImageManager.setWebContents(webContents);
        if (mediaSession != null) {
            mMediaSessionObserver = createMediaSessionObserver(mediaSession);
        }
    }

    private void cleanupMediaSessionObserver() {
        if (mMediaSessionObserver == null) return;
        mMediaSessionObserver.stopObserving();
        mMediaSessionObserver = null;
        mMediaSessionActions = null;
    }

    @VisibleForTesting
    final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onContentChanged(Tab tab) {
            assert tab == mTab;
            setWebContents(tab.getWebContents());
        }

        @Override
        public void onFaviconUpdated(Tab tab, Bitmap icon) {
            assert tab == mTab;
            updateFavicon(icon);
        }

        @Override
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
            assert tab == mTab;

            if (!navigation.hasCommitted() || !navigation.isInMainFrame()
                    || navigation.isSameDocument()) {
                return;
            }

            mOrigin = UrlFormatter.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(
                    GURLUtils.getOrigin(mTab.getUrl()));
            mFavicon = null;
            mPageMediaImage = null;
            mPageMetadata = null;
            // |mCurrentMetadata| selects either |mPageMetadata| or |mFallbackTitle|. As there is no
            // guarantee {@link #onTitleUpdated()} will be called before or after this method,
            // |mFallbackTitle| is not reset in this callback, i.e. relying solely on
            // {@link #onTitleUpdated()}. The following assignment is to keep |mCurrentMetadata| up
            // to date as |mPageMetadata| may have changed.
            mCurrentMetadata = getMetadata();
            mMediaSessionActions = null;

            if (isNotificationHiddingOrHidden()) return;

            mNotificationInfoBuilder.setOrigin(mOrigin);
            mNotificationInfoBuilder.setNotificationLargeIcon(mFavicon);
            mNotificationInfoBuilder.setMediaSessionImage(mPageMediaImage);
            mNotificationInfoBuilder.setMetadata(mCurrentMetadata);
            mNotificationInfoBuilder.setMediaSessionActions(mMediaSessionActions);
            showNotification();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            assert tab == mTab;
            String newFallbackTitle = sanitizeMediaTitle(tab.getTitle());
            if (!TextUtils.equals(mFallbackTitle, newFallbackTitle)) {
                mFallbackTitle = newFallbackTitle;
                updateNotificationMetadata();
            }
        }

        @Override
        public void onShown(Tab tab, @TabSelectionType int type) {
            assert tab == mTab;
            MediaNotificationManager.activateAndroidMediaSession(
                    tab.getId(), R.id.media_playback_notification);
        }

        @Override
        public void onDestroyed(Tab tab) {
            assert mTab == tab;

            cleanupMediaSessionObserver();

            hideNotificationImmediately();
            mTab.removeObserver(this);
            mTab = null;
            if (mLargeIconBridge != null) {
                mLargeIconBridge.destroy();
                mLargeIconBridge = null;
            }
        }
    };

    @VisibleForTesting
    MediaSessionTabHelper(Tab tab) {
        mTab = tab;
        mTab.addObserver(mTabObserver);
        mMediaImageManager =
                new MediaImageManager(MediaNotificationManager.MINIMAL_MEDIA_IMAGE_SIZE_PX,
                        MediaNotificationManager.getIdealMediaImageSize());
        if (mTab.getWebContents() != null) setWebContents(tab.getWebContents());

        Activity activity = getActivityFromTab(mTab);
        if (activity != null) {
            mPreviousVolumeControlStream = activity.getVolumeControlStream();
        }
        mHandler = new Handler();
    }

    /**
     * Creates the {@link MediaSessionTabHelper} for the given {@link Tab}.
     * @param tab the tab to attach the helper to.
     */
    public static void createForTab(Tab tab) {
        new MediaSessionTabHelper(tab);
    }

    /**
     * Removes all the leading/trailing white spaces and the quite common unicode play character.
     * It improves the visibility of the title in the notification.
     *
     * @param title The original tab title, e.g. "   ▶   Foo - Bar  "
     * @return The sanitized tab title, e.g. "Foo - Bar"
     */
    private String sanitizeMediaTitle(String title) {
        title = title.trim();
        return title.startsWith(UNICODE_PLAY_CHARACTER) ? title.substring(1).trim() : title;
    }

    /**
     * Converts the {@link MediaNotificationListener} action source enum into the
     * {@link MediaSessionUMA} one to ensure matching the histogram values.
     * @param source the source id, must be one of the ACTION_SOURCE_* constants defined in the
     *               {@link MediaNotificationListener} interface.
     * @return the corresponding histogram value.
     */
    public static @MediaSessionUMA.MediaSessionActionSource int convertMediaActionSourceToUMA(
            int source) {
        if (source == MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION) {
            return MediaSessionUMA.MediaSessionActionSource.MEDIA_NOTIFICATION;
        } else if (source == MediaNotificationListener.ACTION_SOURCE_MEDIA_SESSION) {
            return MediaSessionUMA.MediaSessionActionSource.MEDIA_SESSION;
        } else if (source == MediaNotificationListener.ACTION_SOURCE_HEADSET_UNPLUG) {
            return MediaSessionUMA.MediaSessionActionSource.HEADSET_UNPLUG;
        }

        assert false;
        return MediaSessionUMA.MediaSessionActionSource.NUM_ENTRIES;
    }

    private Activity getActivityFromTab(Tab tab) {
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return null;

        return windowAndroid.getActivity().get();
    }

    /**
     * Updates the best favicon if the given icon is better and the favicon is shown in
     * notification.
     */
    private void updateFavicon(Bitmap icon) {
        if (icon == null) return;

        mMaybeHasFavicon = true;

        // Store the favicon only if notification is being shown. Otherwise the favicon is
        // obtained from large icon bridge when needed.
        if (isNotificationHiddingOrHidden() || mPageMediaImage != null) return;

        // Disable favicons in notifications for low memory devices on O
        // where the notification icon is optional.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && SysUtils.isLowEndDevice()) return;

        if (!MediaNotificationManager.isBitmapSuitableAsMediaImage(icon)) return;
        if (mFavicon != null && (icon.getWidth() < mFavicon.getWidth()
                                        || icon.getHeight() < mFavicon.getHeight())) {
            return;
        }
        mFavicon = MediaNotificationManager.downscaleIconToIdealSize(icon);
        updateNotificationImage(mFavicon);
    }

    /**
     * Updates the metadata in media notification. This method should be called whenever
     * |mPageMetadata| or |mFallbackTitle| is changed.
     */
    private void updateNotificationMetadata() {
        if (isNotificationHiddingOrHidden()) return;

        MediaMetadata newMetadata = getMetadata();
        if (mCurrentMetadata.equals(newMetadata)) return;

        mCurrentMetadata = newMetadata;
        mNotificationInfoBuilder.setMetadata(mCurrentMetadata);
        showNotification();
    }

    /**
     * @return The up-to-date MediaSession metadata. Returns the cached object like |mPageMetadata|
     * or |mCurrentMetadata| if it reflects the current state. Otherwise will return a new
     * {@link MediaMetadata} object.
     */
    private MediaMetadata getMetadata() {
        String title = mFallbackTitle;
        String artist = "";
        String album = "";
        if (mPageMetadata != null) {
            if (!TextUtils.isEmpty(mPageMetadata.getTitle())) return mPageMetadata;

            artist = mPageMetadata.getArtist();
            album = mPageMetadata.getAlbum();
        }

        if (mCurrentMetadata != null && TextUtils.equals(title, mCurrentMetadata.getTitle())
                && TextUtils.equals(artist, mCurrentMetadata.getArtist())
                && TextUtils.equals(album, mCurrentMetadata.getAlbum())) {
            return mCurrentMetadata;
        }

        return new MediaMetadata(title, artist, album);
    }

    private void updateNotificationActions() {
        if (isNotificationHiddingOrHidden()) return;

        mNotificationInfoBuilder.setMediaSessionActions(mMediaSessionActions);
        showNotification();
    }

    private void updateNotificationPosition() {
        if (isNotificationHiddingOrHidden()) return;

        mNotificationInfoBuilder.setMediaPosition(mMediaPosition);
        showNotification();
    }

    @Override
    public void onImageDownloaded(Bitmap image) {
        mPageMediaImage = MediaNotificationManager.downscaleIconToIdealSize(image);
        mFavicon = null;
        updateNotificationImage(mPageMediaImage);
    }

    private void updateNotificationImage(Bitmap newMediaImage) {
        if (mCurrentMediaImage == newMediaImage) return;

        mCurrentMediaImage = newMediaImage;

        if (isNotificationHiddingOrHidden()) return;
        mNotificationInfoBuilder.setNotificationLargeIcon(mCurrentMediaImage);
        mNotificationInfoBuilder.setMediaSessionImage(mPageMediaImage);
        showNotification();
    }

    private Bitmap getCachedNotificationImage() {
        if (mPageMediaImage != null) return mPageMediaImage;
        if (mFavicon != null) return mFavicon;
        return null;
    }

    /**
     * Fetch favicon image and update the notification when available.
     * @return if the favicon will be updated.
     */
    private boolean fetchFaviconImage() {
        // The page does not have a favicon yet to fetch since onFaviconUpdated was never called.
        // Don't waste time trying to find it.
        if (!mMaybeHasFavicon) return false;

        if (mTab == null) return false;
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return false;
        String pageUrl = webContents.getLastCommittedUrl();
        int size = MediaNotificationManager.MINIMAL_MEDIA_IMAGE_SIZE_PX;
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(mTab.getProfile());
        }
        LargeIconBridge.LargeIconCallback callback = new LargeIconBridge.LargeIconCallback() {
            @Override
            public void onLargeIconAvailable(
                    Bitmap icon, int fallbackColor, boolean isFallbackColorDefault, int iconType) {
                if (isNotificationHiddingOrHidden()) return;
                if (icon == null) {
                    // If we do not have any favicon then make sure we show default sound icon. This
                    // icon is used by notification manager only if we do not show any icon.
                    mNotificationInfoBuilder.setDefaultNotificationLargeIcon(
                            R.drawable.audio_playing_square);
                    showNotification();
                } else {
                    updateFavicon(icon);
                }
            }
        };

        return mLargeIconBridge.getLargeIconForUrl(pageUrl, size, callback);
    }

    private boolean isNotificationHiddingOrHidden() {
        return mNotificationInfoBuilder == null;
    }

    private MediaSession getMediaSession(WebContents contents) {
        return (sOverriddenMediaSession != null) ? sOverriddenMediaSession
                                                 : MediaSession.fromWebContents(contents);
    }
}
