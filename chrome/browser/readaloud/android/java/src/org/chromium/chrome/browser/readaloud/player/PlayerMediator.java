// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PAUSED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PLAYING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.STOPPED;

import android.widget.SeekBar.OnSeekBarChangeListener;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class in charge of updating player UI property model. */
class PlayerMediator implements InteractionHandler {
    private final PlayerCoordinator mCoordinator;
    private final PlayerCoordinator.Delegate mDelegate;
    private final PropertyModel mModel;
    private final PlaybackListener mPlaybackListener =
            new PlaybackListener() {
                @Override
                public void onPlaybackDataChanged(PlaybackData data) {
                    setPlaybackState(data.state());
                    float percent =
                            (float) data.absolutePositionNanos()
                                    / (float) data.totalDurationNanos();
                    mModel.set(PlayerProperties.PROGRESS, percent);
                }
            };

    private Playback mPlayback;

    PlayerMediator(
            PlayerCoordinator coordinator,
            PlayerCoordinator.Delegate delegate,
            PropertyModel model) {
        mCoordinator = coordinator;
        mDelegate = delegate;
        mModel = model;
        mModel.set(PlayerProperties.INTERACTION_HANDLER, this);
    }

    void destroy() {
        if (mPlayback != null) {
            mPlayback.removeListener(mPlaybackListener);
        }
    }

    void setPlayback(@Nullable Playback playback) {
        if (mPlayback != null) {
            mPlayback.removeListener(mPlaybackListener);
        }
        mPlayback = playback;
        if (mPlayback != null) {
            mPlayback.addListener(mPlaybackListener);
            mModel.set(PlayerProperties.TITLE, mPlayback.getMetadata().title());
            mModel.set(PlayerProperties.PUBLISHER, mPlayback.getMetadata().publisher());
        }
    }

    void setPlaybackState(@PlaybackListener.State int currentPlaybackState) {
        mModel.set(PlayerProperties.PLAYBACK_STATE, currentPlaybackState);
    }

    // InteractionHandler implementation
    @Override
    public void onPlayPauseClick() {
        assert mPlayback != null;

        @PlaybackListener.State int state = mModel.get(PlayerProperties.PLAYBACK_STATE);

        // Call playback control methods and rely on updates through mPlaybackListener
        // to update UI with new playback state.
        switch (state) {
            case PLAYING:
                mPlayback.pause();
                return;

            case PAUSED:
            case STOPPED:
                mPlayback.play();
                return;

            default:
                return;
        }
    }

    @Override
    public void onCloseClick() {
        mCoordinator.closeClicked();
    }

    @Override
    public void onPublisherClick() {}

    @Override
    public void onSeekBackClick() {}

    @Override
    public void onSeekForwardClick() {}

    @Override
    public void onVoiceSelected(PlaybackVoice voice) {
        // TODO request playback with new voice
        ReadAloudPrefs.setVoice(
                mDelegate.getPrefService(), voice.getLanguage(), voice.getVoiceId());
    }

    @Override
    public void onPreviewVoiceClick(PlaybackVoice voice) {}

    @Override
    public void onHighlightingChange(boolean enabled) {
        // TODO enable or disable highlighting
        ReadAloudPrefs.setHighlightingEnabled(mDelegate.getPrefService(), enabled);
    }

    @Override
    public OnSeekBarChangeListener getSeekBarChangeListener() {
        // TODO implement
        return null;
    }

    @Override
    public void onSpeedChange(float newSpeed) {
        // TODO change playback speed
        ReadAloudPrefs.setSpeed(mDelegate.getPrefService(), newSpeed);
    }

    @Override
    public void onTranslateLanguageChange(String targetLanguage) {}

    @Override
    public void onMiniPlayerExpandClick() {
        mCoordinator.expand();
    }
}
