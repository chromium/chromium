// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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

    /** Visibility and animation states for the expanded player. */
    @IntDef({State.GONE, State.SHOWING, State.VISIBLE, State.HIDING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        /** Expanded player isn't on the screen or transitioning. Default state. */
        int GONE = 0;
        /** Expanded player is transitioning from GONE to VISIBLE. */
        int SHOWING = 1;
        /** Expanded player is open and not transitioning. */
        int VISIBLE = 2;
        /** Expanded player is transitioning from VISIBLE to GONE. */
        int HIDING = 3;
    }

    /** Returns the expanded player state. */
    default @State int getState() {
        return State.GONE;
    }
}
