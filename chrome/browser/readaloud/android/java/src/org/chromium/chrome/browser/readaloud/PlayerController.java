// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.base.Log;
import org.chromium.chrome.browser.readaloud.miniplayer.MiniPlayerCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.readaloud.ExpandedPlayer;
import org.chromium.chrome.modules.readaloud.Playback;

/**
 * Class that controls and coordinates the mini and expanded player UI.
 *
 * The expanded player is a full-width bottom sheet that will completely obscure
 * the mini player if it's showing. Since showing or hiding the mini player
 * requires resizing web contents which is expensive and laggy, we will leave
 * the mini player on screen when the expanded player is shown.
 *
 * States:
 * A. no players shown
 * B. mini player visible
 * C. expanded player open and mini player visible (behind expanded player)
 */
class PlayerController {
    private static final String TAG = "RAPlayerCtrlr";
    private final ExpandedPlayer mExpandedPlayer;
    private final ExpandedPlayer.Observer mExpandedPlayerObserver;
    private final MiniPlayerCoordinator mMiniPlayer;
    private final MiniPlayerCoordinator.Observer mMiniPlayerObserver;

    private Tab mPlayingTab;
    private Playback mPlayback;

    public PlayerController(MiniPlayerCoordinator miniPlayer, ExpandedPlayer expandedPlayer) {
        mExpandedPlayer = expandedPlayer;
        mExpandedPlayerObserver = new ExpandedPlayer.Observer() {
            @Override
            public void onCloseClicked() {
                stopAndHideAll();
            }
        };
        mExpandedPlayer.addObserver(mExpandedPlayerObserver);

        mMiniPlayer = miniPlayer;
        mMiniPlayerObserver = new MiniPlayerCoordinator.Observer() {
            @Override
            public void onExpandRequested() {
                mExpandedPlayer.show(mPlayback);
                // Don't bother hiding the mini player because it will be covered and hiding is
                // expensive.
            }
            @Override
            public void onCloseClicked() {
                stopAndHideAll();
            }
        };
        mMiniPlayer.addObserver(mMiniPlayerObserver);
    }

    /** Stop playback and stop tracking players. */
    public void destroy() {
        stopAndHideAll();
        mExpandedPlayer.removeObserver(mExpandedPlayerObserver);
        mMiniPlayer.removeObserver(mMiniPlayerObserver);
    }

    /**
     * Show the mini player, called when playback is requested.
     * @param tab Tab to be played.
     */
    public void playTabRequested(Tab tab) {
        mPlayingTab = tab;
        mMiniPlayer.show(shouldAnimateMiniPlayer(), /* playback= */ null);
    }

    /**
     * Update players when playback is ready.
     * @param playback New Playback object.
     */
    public void playbackReady(Playback playback) {
        mPlayback = playback;
        // Show the players with the new Playback if they are supposed to be showing.
        if (isShowing(mExpandedPlayer.getState())) {
            mExpandedPlayer.show(mPlayback);
        }
        if (isShowing(mMiniPlayer.getState())) {
            mMiniPlayer.show(shouldAnimateMiniPlayer(), mPlayback);
        }
    }

    /** Update players when playback fails. */
    public void playbackFailed() {
        Log.e(TAG, "PlayerController.playbackFailed() not implemented.");
    }

    /** Kill playback and hide players. */
    public void stopAndHideAll() {
        if (mPlayback != null) {
            // TODO make sure everything holding this Playback knows it's been released
            mPlayback.release();
        }
        mMiniPlayer.dismiss(shouldAnimateMiniPlayer());
        mExpandedPlayer.dismiss();
    }

    private boolean shouldAnimateMiniPlayer() {
        // If the expanded player is definitely covering the mini player, we can skip
        // animating the mini player show and hide.
        return mExpandedPlayer.getState() != PlayerState.VISIBLE;
    }

    private static boolean isShowing(@PlayerState int state) {
        return state == PlayerState.SHOWING || state == PlayerState.VISIBLE;
    }
}
