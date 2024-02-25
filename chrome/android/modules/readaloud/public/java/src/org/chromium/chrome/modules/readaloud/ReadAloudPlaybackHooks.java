// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import androidx.annotation.Nullable;

import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.contentjs.Extractor;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

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

    /** Creates the Extractor. */
    default Extractor createExtractor() {
        return new Extractor() {};
    }

    /// Voices methods

    /**
     * Check whether initVoices() has been called and the voice methods are ready to use.
     *
     * @return True if initVoices() was called.
     */
    default boolean voicesInitialized() {
        return false;
    }

    /**
     * Initialize the voice list. Should be called once before using getVoicesFor() and
     * getPlaybackVoiceList().
     */
    default void initVoices() {}

    /**
     * Get the list of all voices in the given language. Returns an empty list if
     * currentPageLanguage is null or invalid.
     *
     * @param currentPageLanguage A language.
     * @return All voices for currentPageLanguage.
     */
    default List<PlaybackVoice> getVoicesFor(@Nullable String currentPageLanguage) {
        return new ArrayList<>();
    }

    /**
     * Get the list of voices that should be sent when requesting playback.
     *
     * @param voiceOverrides A map from languages to voice IDs (see ReadAloudPrefs.getVoices()).
     * @return A voice list to attach to a playback request.
     */
    default List<PlaybackVoice> getPlaybackVoiceList(Map<String, String> voiceOverrides) {
        return new ArrayList<>();
    }
}
