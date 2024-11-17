// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import android.content.Context;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BundleUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.chrome.browser.readaloud.player.expanded.ExpandedPlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.mini.MiniPlayerCoordinator;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class that controls and coordinates the mini and expanded player UI.
 *
 * <p>The expanded player is a full-width bottom sheet that will completely obscure the mini player
 * if it's showing. Since showing or hiding the mini player requires resizing web contents which is
 * expensive and laggy, we will leave the mini player on screen when the expanded player is shown.
 *
 * <p>States: A. no players shown B. mini player visible C. expanded player open and mini player
 * visible (behind expanded player)
 */
public class PlayerCoordinator implements Player {
    private static final String TAG = "ReadAloudPlayer";
    private final ObserverList<Observer> mObserverList;
    private final PlayerMediator mMediator;
    private final Delegate mDelegate;
    private final MiniPlayerCoordinator mMiniPlayer;
    private final ExpandedPlayerCoordinator mExpandedPlayer;
    private boolean mRestoreMiniPlayer;
    private boolean mRestoreExpandedPlayer;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    // TODO remove internal call and then remove this constructor
    public PlayerCoordinator(
            Context splitContext, ViewStub miniPlayerViewStubm, Delegate delegate) {
        this(delegate);
    }

    public PlayerCoordinator(Delegate delegate) {
        mObserverList = new ObserverList<Observer>();
        PropertyModel model =
                new PropertyModel.Builder(PlayerProperties.ALL_KEYS)
                        // TODO Set voice and highlighting from settings when needed.
                        .with(
                                PlayerProperties.SPEED,
                                ReadAloudPrefs.getSpeed(delegate.getPrefService()))
                        .build();
        // This Context can be used to inflate views from the split.
        Context contextForInflation =
                BundleUtils.createContextForInflation(
                        delegate.getActivity(), "read_aloud_playback");
        mMiniPlayer =
                new MiniPlayerCoordinator(
                        delegate.getActivity(),
                        contextForInflation,
                        model,
                        delegate.getBottomControlsStacker(),
                        delegate.getLayoutManager(),
                        this,
                        delegate.getUserEducationHelper());
        mMediator = new PlayerMediator(/* coordinator= */ this, delegate, model);
        mExpandedPlayer = new ExpandedPlayerCoordinator(contextForInflation, delegate, model);
        mDelegate = delegate;
        mActivityLifecycleDispatcher = delegate.getActivityLifecycleDispatcher();
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.register(mExpandedPlayer);
        }
    }

    @VisibleForTesting
    PlayerCoordinator(
            MiniPlayerCoordinator miniPlayer,
            PlayerMediator mediator,
            Delegate delegate,
            ExpandedPlayerCoordinator player) {
        mObserverList = new ObserverList<Observer>();
        mMiniPlayer = miniPlayer;
        mMediator = mediator;
        mDelegate = delegate;
        mExpandedPlayer = player;
        mActivityLifecycleDispatcher = delegate.getActivityLifecycleDispatcher();
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.register(mExpandedPlayer);
        }
    }

    @Override
    public void addObserver(Observer observer) {
        mObserverList.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    @Override
    public void destroy() {
        dismissPlayers();
        mMediator.destroy();
        mMiniPlayer.destroy();
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(mExpandedPlayer);
        }
    }

    @Override
    public void playTabRequested() {
        mMediator.setPlayback(null);
        mMediator.setPlaybackState(PlaybackListener.State.BUFFERING);
        if (!mExpandedPlayer.anySheetShowing()) {
            mMiniPlayer.show(/* animate= */ true);
        }
    }

    @Override
    public void playbackReady(Playback playback, @PlaybackListener.State int currentPlaybackState) {
        mMediator.setPlayback(playback);
        mMediator.setPlaybackState(currentPlaybackState);
    }

    @Override
    public void playbackFailed() {
        mMediator.setPlayback(null);
        mMediator.setPlaybackState(PlaybackListener.State.ERROR);
    }

    @Override
    public void recordPlaybackDuration() {
        mMediator.recordPlaybackDuration();
    }

    /** Show expanded player. */
    void expand() {
        if (mDelegate.getProfile() != null) {
            TrackerFactory.getTrackerForProfile(mDelegate.getProfile())
                    .notifyEvent("read_aloud_expanded_player_shown");
        }
        mExpandedPlayer.show();
        mMiniPlayer.dismiss(/* animate= */ false);
    }

    @Override
    public void restoreMiniPlayer() {
        mMiniPlayer.show(/* animate= */ true);
        mMediator.setHiddenAndPlaying(false);
    }

    @Override
    public void dismissPlayers() {
        // Resetting the state. We can do it unconditionally because this UI is only
        // dismissed when stopping the playback.
        mMediator.setPlayback(null);
        mMediator.setPlaybackState(PlaybackListener.State.STOPPED);
        if (!mMediator.isPlayerRestorable()) {
            mMiniPlayer.dismiss(/* animate= */ true);
            mExpandedPlayer.dismiss();
        }
        mMediator.setHiddenAndPlaying(false);
    }

    @Override
    public void hideMiniPlayer() {
        int miniPlayerVisibility = mMiniPlayer.getVisibility();
        if (miniPlayerVisibility == VisibilityState.SHOWING
                || miniPlayerVisibility == VisibilityState.VISIBLE) {
            mMiniPlayer.dismiss(/* animate= */ true);
            mMediator.setHiddenAndPlaying(true);
        }
    }

    /** Collapses the expanded player and shows mini player */
    void hideExpandedPlayer() {
        mExpandedPlayer.dismiss(true);
    }

    @Override
    public void hidePlayers() {
        int expandedSheetVisibility = mExpandedPlayer.getVisibility();
        int miniPlayerVisibility = mMiniPlayer.getVisibility();
        if (expandedSheetVisibility == VisibilityState.SHOWING
                || expandedSheetVisibility == VisibilityState.VISIBLE) {
            mRestoreExpandedPlayer = true;
            mRestoreMiniPlayer = false;
            mExpandedPlayer.dismiss();
        } else if (miniPlayerVisibility == VisibilityState.SHOWING
                || miniPlayerVisibility == VisibilityState.VISIBLE) {
            mRestoreMiniPlayer = true;
            mRestoreExpandedPlayer = false;
            mMiniPlayer.dismiss(/* animate= */ true);
        }

        mMediator.setHiddenAndPlaying(true);
    }

    @Override
    public void restorePlayers() {
        if (mRestoreMiniPlayer) {
            restoreMiniPlayer();
            mRestoreMiniPlayer = false;
        } else if (mRestoreExpandedPlayer) {
            mExpandedPlayer.show();
            mRestoreExpandedPlayer = false;
            mMediator.setHiddenAndPlaying(false);
        }
    }

    @Override
    public void onScreenStatusChanged(boolean isScreenLocked) {
        mMediator.onScreenStatusChanged(isScreenLocked);
    }

    @Override
    public void setPlayerRestorable(boolean isPlayerRestorable) {
        mMediator.setPlayerRestorable(isPlayerRestorable);
    }

    /** To be called when the close button is clicked. */
    void closeClicked() {
        for (Observer o : mObserverList) {
            o.onRequestClosePlayers();
        }
    }

    void voiceMenuClosed() {
        for (Observer o : mObserverList) {
            o.onVoiceMenuClosed();
        }
    }

    // Called by mini player when it finishes showing.
    public void onMiniPlayerShown() {
        for (Observer o : mObserverList) {
            o.onMiniPlayerShown();
        }
    }
}
