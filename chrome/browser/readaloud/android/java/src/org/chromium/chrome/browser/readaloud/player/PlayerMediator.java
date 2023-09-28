// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import android.widget.SeekBar.OnSeekBarChangeListener;

import androidx.annotation.Nullable;

import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class in charge of updating player UI property model. */
class PlayerMediator implements InteractionHandler {
    private final PlayerCoordinator mCoordinator;
    private final PropertyModel mModel;

    PlayerMediator(PlayerCoordinator coordinator, PropertyModel model) {
        mCoordinator = coordinator;
        mModel = model;
        mModel.set(PlayerProperties.INTERACTION_HANDLER, this);
    }

    void destroy() {
        // TODO implement
    }

    void setPlayback(@Nullable Playback playback) {
        // TODO implement
    }

    void setPlaybackState(@PlaybackListener.State int currentPlaybackState) {
        mModel.set(PlayerProperties.PLAYBACK_STATE, currentPlaybackState);
    }

    void updateTitleAndPublisher(String title, String publisher) {
        // TODO implement
    }

    // InteractionHandler implementation
    @Override
    public void onPlayPauseClick() {}

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
    public void onVoiceSelected(PlaybackVoice voice) {}

    @Override
    public void onPreviewVoiceClick(PlaybackVoice voice) {}

    @Override
    public void onHighlightingChange(boolean enabled) {}

    @Override
    public OnSeekBarChangeListener getSeekBarChangeListener() {
        // TODO implement
        return null;
    }

    @Override
    public void onSpeedChange(float newSpeed) {}

    @Override
    public void onTranslateLanguageChange(String targetLanguage) {}

    @Override
    public void onMiniPlayerExpandClick() {}
}
