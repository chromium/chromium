// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import android.widget.SeekBar;

import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;

/** Callbacks for player buttons and seek bar. */
public interface InteractionHandler {
    /** Called when the play/pause button is clicked. */
    void onPlayPauseClick();

    /** Called when the "X" button is clicked. */
    void onCloseClick();

    /** Called when the expanded player publisher button is clicked. */
    void onPublisherClick();

    /** Called when the seek back button is clicked. */
    void onSeekBackClick();

    /** Called when the seek forward button is clicked. */
    void onSeekForwardClick();

    /**
     * Called when a playback voice is chosen.
     * @param voice Selected voice.
     */
    void onVoiceSelected(PlaybackVoice voice);

    /**
     * Called when a voice's "preview voice" button is clicked.
     * @param voice Voice to preview.
     */
    void onPreviewVoiceClick(PlaybackVoice voice);

    /** Called when the voice menu is dismissed. */
    void onVoiceMenuClosed();

    /**
     * Called when the "highlighting enabled" toggle switch is changed.
     * @param enabled Value of switch.
     */
    void onHighlightingChange(boolean enabled);

    /** Listener for seek bar events. */
    SeekBar.OnSeekBarChangeListener getSeekBarChangeListener();

    /**
     * Called when the user changes the playback speed.
     *
     * @param newSpeed New speed.
     */
    void onSpeedChange(float newSpeed);

    /**
     * Called when the user picks a language in the player's translate menu.
     * @param targetLanguage Language to translate to.
     */
    void onTranslateLanguageChange(String targetLanguage);

    /** Called when the user taps somewhere on the mini player to expand it. */
    void onMiniPlayerExpandClick();

    /** Called when the mini player should be hidden, for example when a bottom sheet opens. */
    void onShouldHideMiniPlayer();

    /** Called when the mini player should be restored. */
    void onShouldRestoreMiniPlayer();
}
