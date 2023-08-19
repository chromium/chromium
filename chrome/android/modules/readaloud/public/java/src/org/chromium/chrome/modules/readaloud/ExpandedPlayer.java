// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import org.chromium.chrome.browser.readaloud.PlayerState;

/** Interface for controlling the Read Aloud expanded player. */
public interface ExpandedPlayer {
    /** Interface for getting updates about the expanded player. */
    public interface Observer {
        /** Called when the user has tapped the close button. */
        void onCloseClicked();
    }

    /**
     * Add an observer.
     * @param observer Observer to add.
     */
    default void addObserver(Observer observer) {}

    /**
     * Remove an observer. Has no effect if `observer` wasn't previously added.
     * @param observer Observer to remove.
     */
    default void removeObserver(Observer observer) {}

    /**
     * Show the expanded player.
     *
     * If current state is GONE or HIDING, switch to SHOWING. No effect if state is
     * VISIBLE or SHOWING.
     * @param playback Current playback object. Should not be null.
     */
    default void show(Playback playback) {}

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
