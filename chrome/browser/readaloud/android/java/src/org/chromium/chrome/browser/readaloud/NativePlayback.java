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
 * on-device C++ {@code ReadAloudService} via JNI and driving UI updates via {@link
 * PlaybackListener} observers.
 *
 * <p>This class is not thread-safe and should only be accessed from a single thread. All lifecycle
 * operations and JNI callbacks must run on the Android Main UI thread.
 *
 * <p>Lifecycle & Performance:
 *
 * <ul>
 *   <li>Created by {@link ReadAloudController} when audio playback is requested on an active tab.
 *   <li>Reuses a single anonymous {@link PlaybackListener.PlaybackData} instance across
 *       high-frequency JNI progress notifications ({@code notifyPlaybackProgressUpdated}) to
 *       guarantee zero heap allocations and prevent GC pauses.
 *   <li>Nullifies native pointer via {@code setNativeServicePtr(0)} upon profile or controller
 *       teardown.
 * </ul>
 */
@NullMarked
class NativePlayback implements Playback {
    private final ObserverList<PlaybackListener> mListeners = new ObserverList<>();
    private final NativeMetadata mMetadata;
    private final @Nullable WebContents mWebContents;
    private final PlaybackListener.PlaybackData mPlaybackData;
    private long mNativeServicePtr;
    private @PlaybackListener.State int mState = PlaybackListener.State.BUFFERING;
    private long mAbsolutePositionNanos;
    private long mTotalDurationNanos;

    NativePlayback(
            long nativeServicePtr,
            @Nullable WebContents webContents,
            @Nullable String languageCode,
            @Nullable String canonicalUrl,
            @Nullable PlaybackMode playbackMode) {
        ThreadUtils.assertOnUiThread();
        mNativeServicePtr = nativeServicePtr;
        mWebContents = webContents;
        mMetadata = new NativeMetadata(languageCode, canonicalUrl, playbackMode);
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
     * Called by {@link ReadAloudController} during profile or service teardown (passing 0) to
     * prevent subsequent JNI calls on a destroyed native service.
     */
    void setNativeServicePtr(long nativeServicePtr) {
        ThreadUtils.assertOnUiThread();
        mNativeServicePtr = nativeServicePtr;
    }

    /**
     * Called by {@link ReadAloudController} when the C++ service dispatches {@code
     * OnMetadataAvailable} via JNI with dynamic title and publisher updates.
     */
    void updateMetadata(@Nullable String title, @Nullable String publisher) {
        ThreadUtils.assertOnUiThread();
        mMetadata.setTitle(title);
        mMetadata.setPublisher(publisher);
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
        if (mNativeServicePtr != 0 && mWebContents != null && !mWebContents.isDestroyed()) {
            ReadAloudControllerJni.get().play(mNativeServicePtr, mWebContents);
        }
    }

    @Override
    public void pause() {
        ThreadUtils.assertOnUiThread();
        if (mNativeServicePtr != 0) {
            ReadAloudControllerJni.get().pause(mNativeServicePtr);
        }
    }

    @Override
    public void seekRelative(long seekDurationNanos) {
        ThreadUtils.assertOnUiThread();
        if (mNativeServicePtr != 0) {
            ReadAloudControllerJni.get().seekRelative(mNativeServicePtr, seekDurationNanos);
        }
    }

    @Override
    public void seek(long absolutePositionNanos) {
        ThreadUtils.assertOnUiThread();
        if (mNativeServicePtr != 0) {
            ReadAloudControllerJni.get().seek(mNativeServicePtr, absolutePositionNanos);
        }
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
        if (mNativeServicePtr != 0) {
            ReadAloudControllerJni.get().setPlaybackRate(mNativeServicePtr, rate);
        }
    }

    @Override
    public void release() {
        ThreadUtils.assertOnUiThread();
        if (mNativeServicePtr != 0) {
            ReadAloudControllerJni.get().stop(mNativeServicePtr);
            mNativeServicePtr = 0;
        }
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
        if (mNativeServicePtr != 0) {
            ReadAloudControllerJni.get().sendFeedback(mNativeServicePtr, feedbackType.getValue());
            if (callback != null) {
                // NOTE: Currently invoked synchronously as a placeholder. UI must NOT rely on
                // immediate execution, as this flow will become asynchronous when fully wired.
                callback.onSuccess();
            }
        } else if (callback != null) {
            callback.onFailure(new Exception("Native service pointer is null"));
        }
    }
}
