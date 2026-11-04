// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
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
    @JniType("::readaloud::ReadAloudBridge")
    interface NativeReadAloudBridge extends JniTypeToken {}

    // Borrowed reference to the native C++ ReadAloudBridge instance.
    private @Nullable JniRawPtr<NativeReadAloudBridge> mNativeReadAloudBridge;
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
        if (mNativeReadAloudBridge != null) {
            destroy();
        }
        mController = controller;
        mNativeReadAloudBridge = ReadAloudNativeBridgeJni.get().init(profile, this);
    }

    /** Returns true if the native bridge is initialized and active. */
    public boolean isInitialized() {
        ThreadUtils.assertOnUiThread();
        return mNativeReadAloudBridge != null;
    }

    /** Unregisters the delegate in native and releases the pointer reference. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().destroy(mNativeReadAloudBridge);
            releaseNativePtr();
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

    /** Initializes the native playback session for the given web contents. */
    public void initializeSession(@Nullable WebContents webContents) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null && webContents != null && !webContents.isDestroyed()) {
            ReadAloudNativeBridgeJni.get().initializeSession(mNativeReadAloudBridge, webContents);
        }
    }

    /** Starts or resumes audio playback. */
    public void play(@Nullable WebContents webContents) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null && webContents != null && !webContents.isDestroyed()) {
            ReadAloudNativeBridgeJni.get().play(mNativeReadAloudBridge, webContents);
        }
    }

    /** Pauses active audio playback. */
    public void pause() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().pause(mNativeReadAloudBridge);
        }
    }

    /** Stops audio playback and releases playback resources. */
    public void stop() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().stop(mNativeReadAloudBridge);
        }
    }

    /** Seeks to the word at the specified segment index and character offset. */
    public void seekToWord(int segmentIndex, int characterOffset) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get()
                    .seekToWord(mNativeReadAloudBridge, segmentIndex, characterOffset);
        }
    }

    /** Seeks to a specific absolute time offset from the beginning of the audio. */
    public void seek(long absoluteTimeNanos) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().seek(mNativeReadAloudBridge, absoluteTimeNanos);
        }
    }

    /** Seeks forward or backward relatively (e.g., for skip buttons). */
    public void seekRelative(long offsetNanos) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().seekRelative(mNativeReadAloudBridge, offsetNanos);
        }
    }

    /** Adjusts the audio playback speed (rate multiplier). */
    public void setPlaybackRate(float rate) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().setPlaybackRate(mNativeReadAloudBridge, rate);
        }
    }

    /** Sets the voice to be used for text-to-speech synthesis. */
    public void setVoice(String voiceId) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().setVoice(mNativeReadAloudBridge, voiceId);
        }
    }

    /** Sets the target language code for text-to-speech synthesis. */
    public void setLanguageCode(String languageCode) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().setLanguageCode(mNativeReadAloudBridge, languageCode);
        }
    }

    /** Plays or resumes a short audio sample of the specified voice. */
    public void previewVoice(String voiceId, String previewText) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get()
                    .previewVoice(mNativeReadAloudBridge, voiceId, previewText);
        }
    }

    /** Pauses the active voice preview playback. */
    public void pauseVoicePreview() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().pauseVoicePreview(mNativeReadAloudBridge);
        }
    }

    /** Stops the active voice preview playback. */
    public void stopVoicePreview() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().stopVoicePreview(mNativeReadAloudBridge);
        }
    }

    /** Sets the playback mode (classic full read or summary overview). */
    public void setPlaybackMode(int mode) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().setPlaybackMode(mNativeReadAloudBridge, mode);
        }
    }

    /** Toggles synchronized word highlighting in the UI. */
    public void setHighlightingEnabled(boolean enabled) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().setHighlightingEnabled(mNativeReadAloudBridge, enabled);
        }
    }

    /** Submits user feedback (e.g., thumbs up/down) for logging. */
    public void sendFeedback(int feedbackType) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().sendFeedback(mNativeReadAloudBridge, feedbackType);
        }
    }

    /** Initiates an asynchronous check to determine if the URL is readable. */
    public void checkReadability(@Nullable GURL url) {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null && url != null) {
            ReadAloudNativeBridgeJni.get().checkReadability(mNativeReadAloudBridge, url);
        }
    }

    /** Initializes and fetches the available synthesis voices from native. */
    public void initVoices() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            ReadAloudNativeBridgeJni.get().initVoices(mNativeReadAloudBridge);
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
            @JniType("std::vector<std::string>") String[] voiceLangs,
            @JniType("std::vector<std::string>") String[] voiceRegions,
            @JniType("std::vector<int32_t>") int[] voicePitches,
            @JniType("std::vector<int32_t>") int[] voiceTones) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onVoicesAvailable(
                    voiceIds,
                    voiceDisplayNames,
                    voiceLangs,
                    voiceRegions,
                    voicePitches,
                    voiceTones);
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
    void onTextChunked(@JniType("std::vector<std::u16string>") String[] chunks) {
        ThreadUtils.assertOnUiThread();
        if (mController != null) {
            mController.onTextChunked(chunks);
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
    void releaseNativePtr() {
        ThreadUtils.assertOnUiThread();
        if (mNativeReadAloudBridge != null) {
            mNativeReadAloudBridge.release();
            mNativeReadAloudBridge = null;
        }
    }

    @CalledByNative
    void onNativeDestroyed() {
        ThreadUtils.assertOnUiThread();
        releaseNativePtr();
        if (mController != null) {
            mController.onNativeDestroyed();
        }
    }

    // ============================================================================
    // JNI Native Methods (JNI Zero Contract Interface for C++ Code Generation)
    //
    // Abstract method declarations parsed by Chromium's jni_zero tool to generate
    // the C++ JNI binding code (ReadAloudNativeBridgeJni). Each method accepts
    // JniPtr<NativeReadAloudBridge> as its first argument to map to ReadAloudBridge
    // instance methods in C++.
    // ============================================================================

    @NativeMethods
    interface Natives {
        // Initializes the native ReadAloudBridge C++ instance.
        @Nullable JniRawPtr<NativeReadAloudBridge> init(
                @JniType("Profile*") Profile profile, ReadAloudNativeBridge bridge);

        // Initializes the native playback session for the given web contents without starting audio
        // playback.
        void initializeSession(
                JniPtr<NativeReadAloudBridge> self,
                @JniType("content::WebContents*") WebContents webContents);

        // Starts or resumes audio playback.
        void play(
                JniPtr<NativeReadAloudBridge> self,
                @JniType("content::WebContents*") WebContents webContents);

        // Pauses the current audio playback.
        void pause(JniPtr<NativeReadAloudBridge> self);

        // Stops audio playback and releases playback resources.
        void stop(JniPtr<NativeReadAloudBridge> self);

        // Seeks to the word at the specified segment index and character offset.
        void seekToWord(JniPtr<NativeReadAloudBridge> self, int segmentIndex, int characterOffset);

        // Seeks to a specific absolute time offset from the beginning of the audio.
        void seek(JniPtr<NativeReadAloudBridge> self, long absoluteTimeNanos);

        // Seeks forward or backward relatively.
        void seekRelative(JniPtr<NativeReadAloudBridge> self, long offsetNanos);

        // Adjusts the audio playback speed (rate multiplier).
        void setPlaybackRate(JniPtr<NativeReadAloudBridge> self, float rate);

        // Sets the voice to be used for text-to-speech synthesis.
        void setVoice(JniPtr<NativeReadAloudBridge> self, @JniType("std::string") String voiceId);

        // Sets the target language code for text-to-speech synthesis.
        void setLanguageCode(
                JniPtr<NativeReadAloudBridge> self, @JniType("std::string") String languageCode);

        // Plays or resumes a short audio sample of the specified voice.
        void previewVoice(
                JniPtr<NativeReadAloudBridge> self,
                @JniType("std::string") String voiceId,
                @JniType("std::u16string") String previewText);

        // Pauses the active voice preview playback.
        void pauseVoicePreview(JniPtr<NativeReadAloudBridge> self);

        // Stops the active voice preview playback.
        void stopVoicePreview(JniPtr<NativeReadAloudBridge> self);

        // Sets the playback mode (classic full read or summary overview).
        void setPlaybackMode(JniPtr<NativeReadAloudBridge> self, int mode);

        // Toggles synchronized word highlighting in the UI.
        void setHighlightingEnabled(JniPtr<NativeReadAloudBridge> self, boolean enabled);

        // Submits user feedback (e.g., thumbs up/down) for logging.
        void sendFeedback(JniPtr<NativeReadAloudBridge> self, int feedbackType);

        // Initiates an asynchronous check to determine if the URL is readable.
        void checkReadability(JniPtr<NativeReadAloudBridge> self, @JniType("GURL") GURL url);

        // Initializes and fetches the available synthesis voices from native.
        void initVoices(JniPtr<NativeReadAloudBridge> self);

        // Destroys the native bridge delegate.
        void destroy(JniPtr<NativeReadAloudBridge> self);
    }
}
