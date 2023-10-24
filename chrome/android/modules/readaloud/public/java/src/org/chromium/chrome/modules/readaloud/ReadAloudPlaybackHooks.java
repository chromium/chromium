// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;

/** Interface for creating ReadAloud playback. */
public interface ReadAloudPlaybackHooks {
    /** Interface to receive createPlayback result. */
    interface CreatePlaybackCallback {
        /** Called if createPlayback() succeeds. */
        void onSuccess(Playback playback);
        /** Called if createPlayback() fails. */
        void onFailure(Throwable t);
    }

    /**
     * Request playback for the given URL.
     * TODO(basiaz): delete after the other method is implemented in clank.
     *
     * @param url                      Page URL.
     * @param dateModifiedMsSinceEpoch Page's dateModified attribute in terms of
     *                                 milliseconds since
     *                                 Unix epoch. Pass current time if dateModified
     *                                 isn't available.
     * @param callback                 Called when request finishes or fails.
     */
    default void createPlayback(
            String url, long dateModifiedMsSinceEpoch, CreatePlaybackCallback callback) {}

    /**
     * Request playback for the given config.
     *
     * @param playbackArgs Encapsulates the info about requested playback.
     * @param callback     Called when request finishes or fails.
     */
    default void createPlayback(PlaybackArgs playbackArgs, CreatePlaybackCallback callback) {}

    /**
     * Create the player UI.
     *
     * @param delegate Delegate providing the UI with outside dependencies.
     * @return a Player.
     */
    default Player createPlayer(Player.Delegate delegate) {
        return new Player() {};
    }

    /** Creates the Highlighter. */
    default Highlighter createHighlighter() {
        return new Highlighter() {};
    }
}
