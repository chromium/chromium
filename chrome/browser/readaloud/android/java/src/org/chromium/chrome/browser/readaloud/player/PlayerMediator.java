// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PAUSED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PLAYING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.STOPPED;

import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter.Mode;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Map;

/** Mediator class in charge of updating player UI property model. */
class PlayerMediator implements InteractionHandler {
    private static final long SEEK_BACK_NANOS = -10 * 1_000_000_000L;
    private static final long SEEK_FORWARD_NANOS = 30 * 1_000_000_000L;
    private final PlayerCoordinator mCoordinator;
    private final PlayerCoordinator.Delegate mDelegate;
    private final PropertyModel mModel;
    private final PlaybackListener mPlaybackListener =
            new PlaybackListener() {
                @Override
                public void onPlaybackDataChanged(PlaybackData data) {
                    setPlaybackState(data.state());
                    mModel.set(PlayerProperties.ELAPSED_NANOS, data.absolutePositionNanos());
                    mModel.set(PlayerProperties.DURATION_NANOS, data.totalDurationNanos());
                    float percent =
                            (float) data.absolutePositionNanos()
                                    / (float) data.totalDurationNanos();
                    mModel.set(PlayerProperties.PROGRESS, percent);
                    mModel.set(PlayerProperties.ELAPSED_NANOS, data.absolutePositionNanos());
                    mModel.set(PlayerProperties.DURATION_NANOS, data.totalDurationNanos());
                }
            };
    private final OnSeekBarChangeListener mSeekBarChangeListener =
            new OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    if (!fromUser) {
                        return;
                    }
                    float percent = (float) progress / (float) seekBar.getMax();
                    mPlayback.seek((long) (mModel.get(PlayerProperties.DURATION_NANOS) * percent));
                }

                @Override
                public void onStartTrackingTouch(SeekBar seekBar) {}

                @Override
                public void onStopTrackingTouch(SeekBar seekBar) {}
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

        mDelegate.getCurrentLanguageVoicesSupplier().addObserver(this::setVoices);
        mDelegate.getVoiceIdSupplier().addObserver(this::setVoice);
    }

    void destroy() {
        if (mPlayback != null) {
            mPlayback.removeListener(mPlaybackListener);
        }

        mDelegate.getVoiceIdSupplier().removeObserver(this::setVoice);
        mDelegate.getCurrentLanguageVoicesSupplier().removeObserver(this::setVoices);
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
            onSpeedChange(ReadAloudPrefs.getSpeed(mDelegate.getPrefService()));
            mModel.set(
                    PlayerProperties.HIGHLIGHTING_ENABLED,
                    mDelegate.getHighlightingEnabledSupplier().get());
            mModel.set(
                    PlayerProperties.HIGHLIGHTING_SUPPORTED, mDelegate.isHighlightingSupported());
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

            case STOPPED:
                mPlayback.seek(0L);
                // fall through
            case PAUSED:
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
    public void onSeekBackClick() {
        maybeSeekRelative(SEEK_BACK_NANOS);
    }

    @Override
    public void onSeekForwardClick() {
        maybeSeekRelative(SEEK_FORWARD_NANOS);
    }

    @Override
    public void onVoiceSelected(PlaybackVoice voice) {
        mDelegate.setVoiceOverrideAndApplyToPlayback(voice);
    }

    @Override
    public void onPreviewVoiceClick(PlaybackVoice voice) {
        mDelegate.previewVoice(voice);
    }

    @Override
    public void onVoiceMenuClosed() {
        mCoordinator.voiceMenuClosed();
    }

    @Override
    public void onHighlightingChange(boolean enabled) {
        mDelegate.getHighlightingEnabledSupplier().set(enabled);
    }

    @Override
    public OnSeekBarChangeListener getSeekBarChangeListener() {
        return mSeekBarChangeListener;
    }

    @Override
    public void onSpeedChange(float newSpeed) {
        ReadAloudPrefs.setSpeed(mDelegate.getPrefService(), newSpeed);
        mPlayback.setRate(newSpeed);
        if (newSpeed >= 2.0f) {
            mDelegate.setHighlighterMode(Mode.TEXT_HIGHLIGHTING_MODE_PARAGRAPH);
        } else {
            mDelegate.setHighlighterMode(Mode.TEXT_HIGHLIGHTING_MODE_WORD);
        }
        // Reflect speed change in UI.
        mModel.set(PlayerProperties.SPEED, newSpeed);
    }

    @Override
    public void onTranslateLanguageChange(String targetLanguage) {}

    @Override
    public void onMiniPlayerExpandClick() {
        mCoordinator.expand();
    }

    private void maybeSeekRelative(long nanos) {
        if (mPlayback == null) {
            return;
        }
        if (mModel.get(PlayerProperties.ELAPSED_NANOS) + nanos
                >= mModel.get(PlayerProperties.DURATION_NANOS)) {
            mPlayback.pause();
            mPlayback.seek(mModel.get(PlayerProperties.DURATION_NANOS));
        } else {
            mPlayback.seekRelative(nanos);
        }
    }

    private void setVoices(List<PlaybackVoice> voices) {
        assert voices != null;
        assert !voices.isEmpty();

        mModel.set(PlayerProperties.VOICES_LIST, voices);

        // Ensure voice selection for current language is reflected in UI.
        PlaybackVoice topVoice = voices.get(0);
        String currentLanguage = topVoice.getLanguage();

        // Use first voice if there's no voice preference for the language.
        String voiceId = topVoice.getVoiceId();
        Map<String, String> voicePrefs = ReadAloudPrefs.getVoices(mDelegate.getPrefService());
        if (voicePrefs.containsKey(currentLanguage)) {
            voiceId = voicePrefs.get(currentLanguage);
        }
        mModel.set(PlayerProperties.SELECTED_VOICE_ID, voiceId);
    }

    private void setVoice(String voiceId) {
        assert voiceId != null;

        mModel.set(PlayerProperties.SELECTED_VOICE_ID, voiceId);
    }
}
