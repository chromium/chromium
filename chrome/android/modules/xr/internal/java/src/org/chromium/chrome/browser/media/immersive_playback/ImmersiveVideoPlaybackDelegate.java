// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import org.chromium.build.annotations.NullMarked;

/** Delegate for controlling media playback in XR, shared between components. */
@NullMarked
public interface ImmersiveVideoPlaybackDelegate {
    /**
     * Toggles the playback state between playing and paused.
     *
     * @param isPlaying True if the media is currently playing, false otherwise.
     */
    void togglePlayPause(boolean isPlaying);

    /**
     * Seeks to the specified position.
     *
     * @param positionMs The position to seek to, in milliseconds.
     */
    void seekTo(long positionMs);

    /** Called when the activity should be exited. */
    void onExitImmersivePlayback();
}
