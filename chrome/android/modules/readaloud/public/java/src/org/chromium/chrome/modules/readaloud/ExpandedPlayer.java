// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import org.chromium.chrome.browser.readaloud.PlayerState;

/** Interface for controlling the Read Aloud expanded player. */
public interface ExpandedPlayer {
    /**
     * Bind the player to a Playback object.
     * @param playback Playback object.
     */
    default void setPlayback(Playback playback) {}

    /**
     * Show the expanded player.
     *
     * If current state is GONE or HIDING, switch to SHOWING. No effect if state is
     * VISIBLE or SHOWING.
     */
    default void show() {}

    /**
     * Dismiss the expanded player.
     *
     * If current state is SHOWING or VISIBLE, switch to HIDING. No effect if state
     * is GONE or HIDING.
     */
    default void dismiss() {}

    /** Returns the expanded player state. */
    default @PlayerState int getState() {
        return PlayerState.GONE;
    }
}
