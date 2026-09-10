// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;

/**
 * Session adapter for native Read Aloud voice preview sample playback, bridging the Java {@link
 * Playback} interface to the on-device C++ {@code ReadAloudService} via {@link
 * ReadAloudNativeBridge} and driving UI button updates via {@link PlaybackListener} observers.
 *
 * <p>Lifecycle & Threading:
 *
 * <ul>
 *   <li>Created by {@link ReadAloudController} when voice preview playback is requested.
 *   <li>All lifecycle operations, control commands, and observer updates must run on the UI thread.
 *   <li>Reuses a single anonymous {@link PlaybackListener.PlaybackData} instance to prevent GC
 *       pauses.
 *   <li>Dispatches {@code previewVoice} and {@code stopVoicePreview} commands to {@link
 *       ReadAloudNativeBridge}.
 * </ul>
 */
@NullMarked
class NativeVoicePreviewPlayback implements Playback {
    private final ObserverList<PlaybackListener> mListeners = new ObserverList<>();
    private final NativeMetadata mMetadata;
    private final PlaybackListener.PlaybackData mPlaybackData;
    private final ReadAloudNativeBridge mNativeBridge;
    private final String mVoiceId;
    private @PlaybackListener.State int mState = PlaybackListener.State.BUFFERING;

    NativeVoicePreviewPlayback(
            ReadAloudNativeBridge nativeBridge, @Nullable String languageCode, String voiceId) {
        ThreadUtils.assertOnUiThread();
        mNativeBridge = nativeBridge;
        mVoiceId = voiceId;
        mMetadata =
                new NativeMetadata(
                        languageCode, /* canonicalUrl= */ null, /* playbackMode= */ null);
        mPlaybackData =
                new PlaybackListener.PlaybackData() {
                    @Override
                    public @PlaybackListener.State int state() {
                        return mState;
                    }

                    @Override
                    public int paragraphIndex() {
                        return 0;
                    }

                    @Override
                    public long positionInParagraphNanos() {
                        return 0L;
                    }

                    @Override
                    public long paragraphDurationNanos() {
                        return 0L;
                    }

                    @Override
                    public long absolutePositionNanos() {
                        return 0L;
                    }

                    @Override
                    public long totalDurationNanos() {
                        return 0L;
                    }
                };
    }

    String getVoiceId() {
        ThreadUtils.assertOnUiThread();
        return mVoiceId;
    }

    @PlaybackListener.State
    int getState() {
        ThreadUtils.assertOnUiThread();
        return mState;
    }

    /**
     * Called by {@link ReadAloudController} when the C++ service dispatches preview state
     * transitions (e.g., buffering, playing, stopped, error) via JNI.
     */
    void notifyPlaybackStateChanged(@PlaybackListener.State int state) {
        ThreadUtils.assertOnUiThread();
        mState = state;
        for (PlaybackListener listener : mListeners) {
            listener.onPlaybackDataChanged(mPlaybackData);
        }
    }

    @Override
    public Playback.Metadata getMetadata() {
        ThreadUtils.assertOnUiThread();
        return mMetadata;
    }

    @Override
    public void addListener(PlaybackListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.addObserver(listener);
    }

    @Override
    public void removeListener(PlaybackListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.removeObserver(listener);
    }

    @Override
    public void play() {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.previewVoice(mVoiceId);
    }

    @Override
    public void pause() {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.stopVoicePreview();
    }

    @Override
    public void release() {
        ThreadUtils.assertOnUiThread();
        if (mState != PlaybackListener.State.STOPPED) {
            mNativeBridge.stopVoicePreview();
        }
    }
}
