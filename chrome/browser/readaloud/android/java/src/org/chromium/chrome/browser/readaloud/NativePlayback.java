// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.modules.readaloud.Feedback.FeedbackType;
import org.chromium.chrome.modules.readaloud.Feedback.NegativeFeedbackReason;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.ReadAloudPlaybackHooks.SendFeedbackCallback;
import org.chromium.content_public.browser.WebContents;

/**
 * Session adapter for active tab article audio, bridging the Java {@link Playback} interface to the
 * on-device C++ {@code ReadAloudService} via {@link ReadAloudNativeBridge} and driving UI updates
 * via {@link PlaybackListener} observers.
 *
 * <p>This class is not thread-safe and should only be accessed from a single thread. All lifecycle
 * operations and JNI callbacks must run on the Android Main UI thread.
 *
 * <p>Lifecycle & Performance:
 *
 * <ul>
 *   <li>Created by {@link ReadAloudController} when audio playback is requested on an active tab.
 *   <li>Reuses a single anonymous {@link PlaybackListener.PlaybackData} instance across
 *       high-frequency progress notifications ({@code notifyPlaybackProgressUpdated}) to guarantee
 *       zero heap allocations and prevent GC pauses.
 *   <li>Delegates native control commands and pointer safety to {@link ReadAloudNativeBridge}.
 * </ul>
 */
@NullMarked
class NativePlayback implements Playback {
    private final ObserverList<PlaybackListener> mListeners = new ObserverList<>();
    private final NativeMetadata mMetadata;
    private final @Nullable WebContents mWebContents;
    private final PlaybackListener.PlaybackData mPlaybackData;
    private final ReadAloudNativeBridge mNativeBridge;
    private @PlaybackListener.State int mState = PlaybackListener.State.BUFFERING;
    private long mAbsolutePositionNanos;
    private long mTotalDurationNanos;

    NativePlayback(
            ReadAloudNativeBridge nativeBridge,
            @Nullable WebContents webContents,
            @Nullable String languageCode,
            @Nullable String canonicalUrl,
            @Nullable PlaybackMode playbackMode) {
        ThreadUtils.assertOnUiThread();
        mNativeBridge = nativeBridge;
        mWebContents = webContents;
        mMetadata = new NativeMetadata(languageCode, canonicalUrl, playbackMode);
        mNativeBridge.setPlaybackMode(mMetadata.playbackMode().getValue());
        // Reused across progress updates to prevent heap allocations and GC pauses.
        mPlaybackData =
                new PlaybackListener.PlaybackData() {
                    @Override
                    public @PlaybackListener.State int state() {
                        return mState;
                    }

                    @Override
                    public int paragraphIndex() {
                        // TODO(b/522834235): Populate paragraphIndex from native service when
                        // granular highlighting is supported.
                        return 0;
                    }

                    @Override
                    public long positionInParagraphNanos() {
                        // TODO(b/522834235): Return position relative to the current paragraph once
                        // granular highlighting is supported.
                        return mAbsolutePositionNanos;
                    }

                    @Override
                    public long paragraphDurationNanos() {
                        // TODO(b/522834235): Return actual paragraph duration once granular
                        // highlighting is supported.
                        return mTotalDurationNanos;
                    }

                    @Override
                    public long absolutePositionNanos() {
                        // Dynamically updated via notifyPlaybackProgressUpdated during C++ audio
                        // synthesis.
                        return mAbsolutePositionNanos;
                    }

                    @Override
                    public long totalDurationNanos() {
                        return mTotalDurationNanos;
                    }
                };
    }

    /**
     * Called by {@link ReadAloudController} when the C++ service dispatches {@code
     * OnMetadataAvailable} via JNI with dynamic title and publisher updates.
     */
    void updateMetadata(@Nullable String title, @Nullable String publisher) {
        ThreadUtils.assertOnUiThread();
        mMetadata.setTitle(title);
        mMetadata.setPublisher(publisher);
        notifyMetadataChanged();
    }

    @PlaybackListener.State
    int getState() {
        ThreadUtils.assertOnUiThread();
        return mState;
    }

    /**
     * Called by {@link ReadAloudController} when the C++ service dispatches audio state transitions
     * (e.g., playing, paused, buffering) via JNI.
     */
    void notifyPlaybackStateChanged(@PlaybackListener.State int state) {
        ThreadUtils.assertOnUiThread();
        mState = state;
        notifyPlaybackDataChanged();
    }

    /**
     * Called by {@link ReadAloudController} when the C++ service dispatches high-frequency audio
     * progress updates via JNI.
     */
    void notifyPlaybackProgressUpdated(long elapsedNanos, long durationNanos) {
        ThreadUtils.assertOnUiThread();
        mAbsolutePositionNanos = elapsedNanos;
        mTotalDurationNanos = durationNanos;
        notifyPlaybackDataChanged();
    }

    private void notifyPlaybackDataChanged() {
        for (PlaybackListener listener : mListeners) {
            listener.onPlaybackDataChanged(mPlaybackData);
        }
    }

    private void notifyMetadataChanged() {
        for (PlaybackListener listener : mListeners) {
            listener.onMetadataChanged(mMetadata);
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
        // Immediately emit current data so newly subscribed UI observers can render initial state.
        listener.onPlaybackDataChanged(mPlaybackData);
    }

    @Override
    public void removeListener(PlaybackListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.removeObserver(listener);
    }

    @Override
    public void play() {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.play(mWebContents);
    }

    @Override
    public void pause() {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.pause();
    }

    @Override
    public void seekRelative(long seekDurationNanos) {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.seekRelative(seekDurationNanos);
    }

    @Override
    public void seek(long absolutePositionNanos) {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.seek(absolutePositionNanos);
    }

    @Override
    public void seekToParagraph(int paragraphIndex, long offsetNanos) {
        ThreadUtils.assertOnUiThread();
        // TODO(b/522834235): Implement paragraph seeking in the native service.
    }

    @Override
    public void seekToWord(int paragraphIndex, int wordIndex) {
        ThreadUtils.assertOnUiThread();
        // TODO(b/522834235): Implement word seeking in the native service.
    }

    @Override
    public void setRate(float rate) {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.setPlaybackRate(rate);
    }

    @Override
    public void release() {
        ThreadUtils.assertOnUiThread();
        mNativeBridge.stop();
        mListeners.clear();
    }

    @Override
    public void sendFeedback(
            FeedbackType feedbackType,
            NegativeFeedbackReason negativeFeedbackReason,
            @Nullable SendFeedbackCallback callback) {
        ThreadUtils.assertOnUiThread();
        // TODO(b/522834235): Pass negativeFeedbackReason to native service and invoke callback via
        // JNI once the native feedback operation completes.
        if (mNativeBridge.isInitialized()) {
            mNativeBridge.sendFeedback(feedbackType.getValue());
            if (callback != null) {
                // NOTE: Currently invoked synchronously as a placeholder. UI must NOT rely on
                // immediate execution, as this flow will become asynchronous when fully wired.
                callback.onSuccess();
            }
        } else if (callback != null) {
            callback.onFailure(new Exception("Native bridge is not initialized"));
        }
    }
}
