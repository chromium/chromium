// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Dedicated JNI bridge class managing 2-way communications between Java and C++ native ReadAloud.
 *
 * <p>Responsibilities:
 *
 * <ul>
 *   <li>Encapsulates native C++ control methods (play, pause, stop, seek, etc.) with null pointer
 *       checks on {@code mNativeReadAloudBridge}.
 *   <li>Receives JNI {@code @CalledByNative} callbacks from C++ and forwards them to {@link
 *       ReadAloudController}.
 * </ul>
 *
 * <p>Lifecycle:
 *
 * <ul>
 *   <li><b>Instantiation:</b> Created by {@link ReadAloudController} during setup.
 *   <li><b>Initialization:</b> {@link #initialize(Profile, ReadAloudController)} is called when
 *       profile becomes available, acquiring the native C++ pointer address and registering the
 *       controller delegate.
 *   <li><b>Active Usage:</b> Dispatches outbound control commands to C++ and receives inbound
 *       status callbacks.
 *   <li><b>Teardown:</b> Cleaned up via {@link #destroy()} during Java destruction or via {@link
 *       #onNativeDestroyed()} when C++ native service is destroyed.
 * </ul>
 */
@JNINamespace("readaloud")
@NullMarked
class ReadAloudNativeBridge {
    // Address of the native C++ ReadAloudBridge instance (for handling Java -> native calls).
    private long mNativeReadAloudBridge;
    // ReadAloudController reference (for handling native -> Java calls).
    private @Nullable ReadAloudController mController;

    /**
     * Initializes the native C++ ReadAloudBridge binding.
     *
     * <p>Acquires the native bridge pointer for the given {@link Profile} and registers the {@link
     * ReadAloudController} instance as the delegate for C++ to Java callbacks.
     *
     * @param profile Active user profile.
     * @param controller Controller instance acting as the native service delegate.
     */
    public void initialize(Profile profile, ReadAloudController controller) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            destroy();
        }
        mController = controller;
        mNativeReadAloudBridge = ReadAloudNativeBridgeJni.get().init(profile, this);
    }

    /** Returns true if the native bridge is initialized and active. */
    public boolean isInitialized() {
        ThreadUtils.assertOnUiThread();
        return mNativeReadAloudBridge != 0;
    }

    /** Unregisters the delegate in native and resets the pointer to 0. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().destroy(mNativeReadAloudBridge);
            mNativeReadAloudBridge = 0;
        }
        mController = null;
    }

    // ============================================================================
    // Java -> C++ Public Control API (Safe Wrappers)
    //
    // Public instance methods called by Java components (e.g. NativePlayback). These
    // wrappers enforce thread safety and verify mNativeReadAloudBridge pointer validity
    // before supplying the native handle to JNI Zero.
    // ============================================================================

    /** Starts or resumes audio playback. */
    public void play(@Nullable WebContents webContents) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0 && webContents != null && !webContents.isDestroyed()) {
            ReadAloudNativeBridgeJni.get().play(mNativeReadAloudBridge, webContents);
        }
    }

    /** Pauses active audio playback. */
    public void pause() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().pause(mNativeReadAloudBridge);
        }
    }

    /** Stops audio playback and releases playback resources. */
    public void stop() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().stop(mNativeReadAloudBridge);
        }
    }

    /** Seeks to the start of the word at the specified index in the text. */
    public void seekToWordIndex(int wordIndex) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().seekToWordIndex(mNativeReadAloudBridge, wordIndex);
        }
    }

    /** Seeks to a specific absolute time offset from the beginning of the audio. */
    public void seek(long absoluteTimeNanos) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().seek(mNativeReadAloudBridge, absoluteTimeNanos);
        }
    }

    /** Seeks forward or backward relatively (e.g., for skip buttons). */
    public void seekRelative(long offsetNanos) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().seekRelative(mNativeReadAloudBridge, offsetNanos);
        }
    }

    /** Adjusts the audio playback speed (rate multiplier). */
    public void setPlaybackRate(float rate) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().setPlaybackRate(mNativeReadAloudBridge, rate);
        }
    }

    /** Sets the voice to be used for text-to-speech synthesis. */
    public void setVoice(String voiceId) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().setVoice(mNativeReadAloudBridge, voiceId);
        }
    }

    /** Plays a short audio sample of the specified voice. */
    public void previewVoice(String voiceId) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().previewVoice(mNativeReadAloudBridge, voiceId);
        }
    }

    /** Stops the active voice preview playback. */
    public void stopVoicePreview() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().stopVoicePreview(mNativeReadAloudBridge);
        }
    }

    /** Sets the playback mode (classic full read or summary overview). */
    public void setPlaybackMode(int mode) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().setPlaybackMode(mNativeReadAloudBridge, mode);
        }
    }

    /** Toggles synchronized word highlighting in the UI. */
    public void setHighlightingEnabled(boolean enabled) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().setHighlightingEnabled(mNativeReadAloudBridge, enabled);
        }
    }

    /** Submits user feedback (e.g., thumbs up/down) for logging. */
    public void sendFeedback(int feedbackType) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0) {
            ReadAloudNativeBridgeJni.get().sendFeedback(mNativeReadAloudBridge, feedbackType);
        }
    }

    /** Initiates an asynchronous check to determine if the URL is readable. */
    public void checkReadability(@Nullable GURL url) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != 0 && url != null) {
            ReadAloudNativeBridgeJni.get().checkReadability(mNativeReadAloudBridge, url);
        }
    }

    // ============================================================================
    // C++ -> Java Inbound Callbacks (@CalledByNative)
    // ============================================================================

    @CalledByNative
    void onMetadataAvailable(
            @JniType("std::string") String title, @JniType("std::string") String publisher) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onMetadataAvailable(title, publisher);
        }
    }

    @CalledByNative
    void onPlaybackProgressUpdated(long elapsedNanos, long durationNanos) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onPlaybackProgressUpdated(elapsedNanos, durationNanos);
        }
    }

    @CalledByNative
    void onPlaybackStateChanged(int playbackState) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onPlaybackStateChanged(playbackState);
        }
    }

    @CalledByNative
    void onVoicesAvailable(
            @JniType("std::vector<std::string>") String[] voiceIds,
            @JniType("std::vector<std::string>") String[] voiceDisplayNames,
            @JniType("std::string") String selectedVoiceId) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onVoicesAvailable(voiceIds, voiceDisplayNames, selectedVoiceId);
        }
    }

    @CalledByNative
    void onWordHighlightUpdated(int absoluteStartIndex, int absoluteEndIndex) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onWordHighlightUpdated(absoluteStartIndex, absoluteEndIndex);
        }
    }

    @CalledByNative
    void onHighlightingSupported(boolean supported) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onHighlightingSupported(supported);
        }
    }

    @CalledByNative
    void onFallbackEngaged() {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onFallbackEngaged();
        }
    }

    @CalledByNative
    void onPlaybackError(@JniType("std::string") String errorMessage) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onPlaybackError(errorMessage);
        }
    }

    @CalledByNative
    void onVoicePreviewPlaybackStateChanged(
            @JniType("std::string") String voiceId, int playbackState) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onVoicePreviewPlaybackStateChanged(voiceId, playbackState);
        }
    }

    @CalledByNative
    void onReadabilityResult(@JniType("GURL") GURL url, boolean isReadable) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onReadabilityResult(url, isReadable);
        }
    }

    @CalledByNative
    void onNativeDestroyed() {
        ThreadUtils.assertOnUiThread();
        mNativeReadAloudBridge = 0;
        if (mController != null) {
            mController.onNativeDestroyed();
        }
    }

    // ============================================================================
    // JNI Native Methods (JNI Zero Contract Interface for C++ Code Generation)
    //
    // Abstract method declarations parsed by Chromium's jni_zero tool to generate
    // the C++ JNI binding code (ReadAloudNativeBridgeJni). Each method accepts
    // long nativeReadAloudBridge as its first argument to map to ReadAloudBridge
    // instance methods in C++.
    // ============================================================================

    @NativeMethods
    interface Natives {
        // Initializes the native ReadAloudBridge C++ instance.
        long init(@JniType("Profile*") Profile profile, ReadAloudNativeBridge bridge);

        // Starts or resumes audio playback.
        void play(
                long nativeReadAloudBridge,
                @JniType("content::WebContents*") WebContents webContents);

        // Pauses the current audio playback.
        void pause(long nativeReadAloudBridge);

        // Stops audio playback and releases playback resources.
        void stop(long nativeReadAloudBridge);

        // Seeks to the start of the word at the specified index in the text.
        void seekToWordIndex(long nativeReadAloudBridge, int wordIndex);

        // Seeks to a specific absolute time offset from the beginning of the audio.
        void seek(long nativeReadAloudBridge, long absoluteTimeNanos);

        // Seeks forward or backward relatively.
        void seekRelative(long nativeReadAloudBridge, long offsetNanos);

        // Adjusts the audio playback speed (rate multiplier).
        void setPlaybackRate(long nativeReadAloudBridge, float rate);

        // Sets the voice to be used for text-to-speech synthesis.
        void setVoice(long nativeReadAloudBridge, @JniType("std::string") String voiceId);

        // Plays a short audio sample of the specified voice.
        void previewVoice(long nativeReadAloudBridge, @JniType("std::string") String voiceId);

        // Stops the active voice preview playback.
        void stopVoicePreview(long nativeReadAloudBridge);

        // Sets the playback mode (classic full read or summary overview).
        void setPlaybackMode(long nativeReadAloudBridge, int mode);

        // Toggles synchronized word highlighting in the UI.
        void setHighlightingEnabled(long nativeReadAloudBridge, boolean enabled);

        // Submits user feedback (e.g., thumbs up/down) for logging.
        void sendFeedback(long nativeReadAloudBridge, int feedbackType);

        // Initiates an asynchronous check to determine if the URL is readable.
        void checkReadability(long nativeReadAloudBridge, @JniType("GURL") GURL url);

        // Destroys the native bridge delegate.
        void destroy(long nativeReadAloudBridge);
    }
}
