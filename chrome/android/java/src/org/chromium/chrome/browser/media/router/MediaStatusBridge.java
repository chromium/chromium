// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import com.google.android.gms.cast.MediaInfo;
import com.google.android.gms.cast.MediaMetadata;
import com.google.android.gms.cast.MediaStatus;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Wrapper layer that exposes a gms.cast.MediaStatus to native code.
 * See also media/base/media_status.h.
 */
@JNINamespace("media_router")
public class MediaStatusBridge {
    private MediaStatus mStatus;

    public MediaStatusBridge(MediaStatus status) {
        mStatus = status;
    }

    /**
     * Gets the play state of the stream. Return values are defined as such:
     * - PLAYER_STATE_UNKOWN = 0
     * - PLAYER_STATE_IDLE = 1
     * - PLAYER_STATE_PLAYING = 2
     * - PLAYER_STATE_PAUSED = 3
     * - PLAYER_STATE_BUFFERING = 4
     * See https://developers.google.com/android/reference/com/google/android/gms/cast/MediaStatus
     */
    @CalledByNative
    public int playerState() {
        return mStatus.getPlayerState();
    }

    /**
     * Gets the idle reason. Only meaningful if we are in PLAYER_STATE_IDLE.
     * - IDLE_REASON_NONE = 0
     * - IDLE_REASON_FINISHED = 1
     * - IDLE_REASON_CANCELED = 2
     * - IDLE_REASON_INTERRUPTED = 3
     * - IDLE_REASON_ERROR = 4
     * See https://developers.google.com/android/reference/com/google/android/gms/cast/MediaStatus
     */
    @CalledByNative
    public int idleReason() {
        return mStatus.getIdleReason();
    }

    /**
     * The main title of the media. For example, in a MediaStatus representing
     * a YouTube Cast session, this could be the title of the video.
     */
    @CalledByNative
    public String title() {
        MediaInfo info = mStatus.getMediaInfo();
        if(info == null)
            return "";

        MediaMetadata metadata = info.getMetadata();
        if(metadata == null)
            return "";

        return metadata.getString(MediaMetadata.KEY_TITLE);
    }

    /**
     * If this is true, the media can be played and paused.
     */
    @CalledByNative
    public boolean canPlayPause() {
        return mStatus.isMediaCommandSupported(MediaStatus.COMMAND_PAUSE);
    }

    /**
     * If this is true, the media can be muted and unmuted.
     */
    @CalledByNative
    public boolean canMute() {
        return mStatus.isMediaCommandSupported(MediaStatus.COMMAND_TOGGLE_MUTE);
    }

    /**
     * If this is true, the media's volume can be changed.
     */
    @CalledByNative
    public boolean canSetVolume() {
        return mStatus.isMediaCommandSupported(MediaStatus.COMMAND_SET_VOLUME);
    }

    /**
     * If this is true, the media's current playback position can be chaxnged.
     */
    @CalledByNative
    public boolean canSeek() {
        return mStatus.isMediaCommandSupported(MediaStatus.COMMAND_SEEK);
    }

    /**
     * Returns the stream's mute state.
     */
    @CalledByNative
    public boolean isMuted() {
        return mStatus.isMute();
    }

    /**
     * Current volume of the media, with 1 being the highest and 0 being the
     * lowest/no sound. When |is_muted| is true, there should be no sound
     * regardless of |volume|.
     */
    @CalledByNative
    public double volume() {
        return mStatus.getStreamVolume();
    }

    /**
     * The length of the media, in ms. A value of zero indicates that this is a media
     * with no set duration (e.g. a live stream).
     */
    @CalledByNative
    public long duration() {
        MediaInfo info = mStatus.getMediaInfo();
        if(info == null)
            return 0;

        return info.getStreamDuration();
    }

    /**
     * Current playback position, in ms. Must be less than or equal to |duration|.
     */
    @CalledByNative
    public long currentTime() {
        return mStatus.getStreamPosition();
    }
}
