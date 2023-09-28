// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import android.content.Context;
import android.view.ViewStub;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.readaloud.player.mini.MiniPlayerCoordinator;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

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
 *
 */
public class PlayerCoordinator {
    private static final String TAG = "ReadAloudPlayer";

    private final ObserverList<Observer> mObserverList;
    private final PropertyModel mModel;
    private final PlayerMediator mMediator;
    private final MiniPlayerCoordinator mMiniPlayer;

    public interface Observer {
        /*
         * Called when the user dismisses the player. The observer is responsible for
         * then calling dismissPlayers().
         */
        void onRequestClosePlayers();
    }

    public PlayerCoordinator(Context context, ViewStub miniPlayerStub) {
        mObserverList = new ObserverList<Observer>();
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS)
                         .with(PlayerProperties.MINI_PLAYER_VISIBILITY, VisibilityState.GONE)
                         .with(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.BUFFERING)
                         .build();
        mMiniPlayer = new MiniPlayerCoordinator(miniPlayerStub, mModel);
        mMediator = new PlayerMediator(/*coordinator=*/this, mModel);
    }

    /**
     * Add an observer to receive event updates.
     *
     * @param observer Observer to add.
     */
    public void addObserver(Observer observer) {
        mObserverList.addObserver(observer);
    }

    /**
     * Remove an observer that was previously added. No effect if the observer was
     * never added.
     */
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    /** Stop playback and stop tracking players. */
    public void destroy() {
        dismissPlayers();
        mMediator.destroy();
    }

    /**
     * Show the mini player, called when playback is requested.
     */
    public void playTabRequested() {
        mMediator.setPlaybackState(PlaybackListener.State.BUFFERING);
        mMiniPlayer.show(shouldAnimateMiniPlayer());
    }

    /**
     * Update players when playback is ready.
     *
     * @param playback             New Playback object.
     * @param currentPlaybackState Playback state.
     */
    public void playbackReady(Playback playback, @PlaybackListener.State int currentPlaybackState) {
        // TODO bind playback
        mMediator.setPlaybackState(currentPlaybackState);
    }

    /** Update players when playback fails. */
    public void playbackFailed() {
        // TODO unbind playback
        mMediator.setPlaybackState(PlaybackListener.State.ERROR);
    }

    /** Show expanded player. */
    void expand() {
        // TODO implement
    }

    /** Hide players. */
    public void dismissPlayers() {
        // Resetting the state. We can do it unconditionally because this UI is only
        // dismissed when stopping the playback.
        mMediator.setPlaybackState(PlaybackListener.State.STOPPED);
        mMiniPlayer.dismiss(shouldAnimateMiniPlayer());
        // TODO dismiss expanded player
    }

    /** To be called when the close button is clicked. */
    void closeClicked() {
        for (Observer o : mObserverList) {
            o.onRequestClosePlayers();
        }
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    private boolean shouldAnimateMiniPlayer() {
        // If the expanded player is definitely covering the mini player, we can skip
        // animating the mini player show and hide.
        // TODO return !mExpandedPlayer.isVisible();
        return true;
    }
}
