// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.MediaInfo;
import com.google.android.gms.cast.MediaStatus;
import com.google.android.gms.cast.RemoteMediaPlayer;
import com.google.android.gms.cast.RemoteMediaPlayer.MediaChannelResult;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.ResultCallback;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.CastSessionUtil;
import org.chromium.chrome.browser.media.router.FlingingController;
import org.chromium.chrome.browser.media.router.MediaController;
import org.chromium.chrome.browser.media.router.MediaStatusBridge;
import org.chromium.chrome.browser.media.router.MediaStatusObserver;
import org.chromium.chrome.browser.media.ui.MediaNotificationInfo;
import org.chromium.chrome.browser.media.ui.MediaNotificationManager;

/**
 * A wrapper around a RemoteMediaPlayer that exposes simple playback commands without the
 * the complexities of the GMS cast calls.
 */
public class RemoteMediaPlayerWrapper implements RemoteMediaPlayer.OnMetadataUpdatedListener,
                                                 RemoteMediaPlayer.OnStatusUpdatedListener,
                                                 ResultCallback<MediaChannelResult>,
                                                 MediaController, FlingingController {
    private static final String TAG = "MediaRemoting";

    private final CastDevice mCastDevice;
    private final String mMediaUrl;

    private GoogleApiClient mApiClient;
    private RemoteMediaPlayer mMediaPlayer;
    private MediaNotificationInfo.Builder mNotificationBuilder;
    private MediaStatusObserver mMediaStatusObserver;

    private boolean mLoaded;

    public RemoteMediaPlayerWrapper(GoogleApiClient apiClient,
            MediaNotificationInfo.Builder notificationBuilder, CastDevice castDevice,
            String mediaUrl) {
        mApiClient = apiClient;
        mCastDevice = castDevice;
        mMediaUrl = mediaUrl;
        mLoaded = false;
        mNotificationBuilder = notificationBuilder;

        mMediaPlayer = new RemoteMediaPlayer();
        mMediaPlayer.setOnStatusUpdatedListener(this);
        mMediaPlayer.setOnMetadataUpdatedListener(this);

        updateNotificationMetadata();
    }

    private void updateNotificationMetadata() {
        CastSessionUtil.setNotificationMetadata(mNotificationBuilder, mCastDevice, mMediaPlayer);
        MediaNotificationManager.show(mNotificationBuilder.build());
    }

    private boolean canSendCommand() {
        return mApiClient != null && mMediaPlayer != null && mApiClient.isConnected();
    }

    // RemoteMediaPlayer.OnStatusUpdatedListener implementation.
    @Override
    public void onStatusUpdated() {
        MediaStatus mediaStatus = mMediaPlayer.getMediaStatus();
        if (mediaStatus == null) return;

        if (mMediaStatusObserver != null) {
            mMediaStatusObserver.onMediaStatusUpdate(new MediaStatusBridge(mediaStatus));
        }

        int playerState = mediaStatus.getPlayerState();
        if (playerState == MediaStatus.PLAYER_STATE_PAUSED
                || playerState == MediaStatus.PLAYER_STATE_PLAYING) {
            mNotificationBuilder.setPaused(playerState != MediaStatus.PLAYER_STATE_PLAYING);
            mNotificationBuilder.setActions(
                    MediaNotificationInfo.ACTION_STOP | MediaNotificationInfo.ACTION_PLAY_PAUSE);
        } else {
            mNotificationBuilder.setActions(MediaNotificationInfo.ACTION_STOP);
        }
        MediaNotificationManager.show(mNotificationBuilder.build());
    }

    // RemoteMediaPlayer.OnMetadataUpdatedListener implementation.
    @Override
    public void onMetadataUpdated() {
        updateNotificationMetadata();
    }

    /**
     * Forwards the message to the underlying RemoteMediaPlayer.
     */
    public void onMediaMessage(String message) {
        if (mMediaPlayer == null) return;

        try {
            mMediaPlayer.onMessageReceived(mCastDevice, CastSessionUtil.MEDIA_NAMESPACE, message);
        } catch (IllegalStateException e) {
            // GMS throws with "Result already set" when receiving responses from multiple API calls
            // in a short amount of time, before results can be read. See https://crbug.com/853923.
        }
    }

    /**
     * Starts loading the media URL, from the given position.
     */
    public void load(long startTime) {
        if (!canSendCommand()) return;

        mLoaded = true;

        MediaInfo.Builder mediaInfoBuilder =
                new MediaInfo.Builder(mMediaUrl).setContentType("*/*").setStreamType(
                        MediaInfo.STREAM_TYPE_BUFFERED);

        mMediaPlayer.load(mApiClient, mediaInfoBuilder.build(), /* autoplay */ true, startTime)
                .setResultCallback(this);
    }

    /**
     * Starts playback. No-op if are not in a valid state.
     * Doesn't verify the command's success/failure.
     */
    @Override
    public void play() {
        if (!canSendCommand()) return;

        if (!mLoaded) {
            load(/* startTime */ 0);
            return;
        }

        try {
            mMediaPlayer.play(mApiClient).setResultCallback(this);
        } catch (IllegalStateException e) {
            // GMS throws with message "Result already set" when making multiple API calls
            // in a short amount of time, before results can be read. See https://crbug.com/853923.
        }
    }

    /**
     * Pauses playback. No-op if are not in a valid state.
     * Doesn't verify the command's success/failure.
     */
    @Override
    public void pause() {
        if (!canSendCommand()) return;

        try {
            mMediaPlayer.pause(mApiClient).setResultCallback(this);
        } catch (IllegalStateException e) {
            // GMS throws with message "Result already set" when making multiple API calls
            // in a short amount of time, before results can be read. See https://crbug.com/853923.
        }
    }

    /**
     * Sets the mute state. Does not affect the stream volume.
     * No-op if are not in a valid state. Doesn't verify the command's success/failure.
     */
    @Override
    public void setMute(boolean mute) {
        if (!canSendCommand()) return;

        try {
            mMediaPlayer.setStreamMute(mApiClient, mute).setResultCallback(this);
        } catch (IllegalStateException e) {
            // GMS throws with message "Result already set" when making multiple API calls
            // in a short amount of time, before results can be read. See https://crbug.com/853923.
        }
    }

    /**
     * Sets the stream volume. Does not affect the mute state.
     * No-op if are not in a valid state. Doesn't verify the command's success/failure.
     */
    @Override
    public void setVolume(double volume) {
        if (!canSendCommand()) return;

        try {
            mMediaPlayer.setStreamVolume(mApiClient, volume).setResultCallback(this);
        } catch (IllegalStateException e) {
            // GMS throws with message "Result already set" when making multiple API calls
            // in a short amount of time, before results can be read. See https://crbug.com/853923.
        }
    }

    /**
     * Seeks to the given position (in milliseconds).
     * No-op if are not in a valid state. Doesn't verify the command's success/failure.
     */
    @Override
    public void seek(long position) {
        if (!canSendCommand()) return;

        if (!mLoaded) {
            load(position);
            return;
        }

        try {
            mMediaPlayer.seek(mApiClient, position).setResultCallback(this);
        } catch (IllegalStateException e) {
            // GMS throws with message "Result already set" when making multiple API calls
            // in a short amount of time, before results can be read. See https://crbug.com/853923.
        }
    }

    /**
     * Called when the session has stopped, and we should no longer send commands.
     */
    public void clearApiClient() {
        mApiClient = null;
    }

    // ResultCallback<MediaChannelResult> implementation
    @Override
    public void onResult(MediaChannelResult result) {
        // When multiple API calls are made in quick succession, "Results have already been set"
        // IllegalStateExceptions might be thrown from GMS code. We prefer to catch the exception
        // and noop it, than to crash. This might lead to some API calls never getting their
        // onResult() called, so we should not rely on onResult() being called for every API call.
        // See https://crbug.com/853923.
        if (!result.getStatus().isSuccess()) {
            Log.e(TAG, "Error when sending command. Status code: %d",
                    result.getStatus().getStatusCode());
        }
    }

    // FlingingController implementation
    @Override
    public MediaController getMediaController() {
        return this;
    }

    @Override
    public long getApproximateCurrentTime() {
        return mMediaPlayer.getApproximateStreamPosition();
    }

    @Override
    public void setMediaStatusObserver(MediaStatusObserver observer) {
        assert mMediaStatusObserver == null;
        mMediaStatusObserver = observer;
    }

    @Override
    public void clearMediaStatusObserver() {
        assert mMediaStatusObserver != null;
        mMediaStatusObserver = null;
    }
}
