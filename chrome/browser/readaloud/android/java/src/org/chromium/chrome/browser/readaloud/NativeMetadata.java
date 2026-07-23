// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;

/**
 * Holds metadata for on-device native Read Aloud playback sessions backed by C++ ReadAloudService.
 *
 * <p>Lifecycle:
 *
 * <ul>
 *   <li>Instantiated when a NativePlayback or NativeVoicePreviewPlayback session is created.
 *   <li>Initialized with immutable request parameters (language code, canonical URL, and playback
 *       mode).
 *   <li>Dynamically updated with title and publisher when the native ReadAloudService dispatches
 *       OnMetadataAvailable via JNI to ReadAloudController.
 *   <li>Persists for the duration of the playback session until the session is released.
 * </ul>
 */
@NullMarked
class NativeMetadata implements Playback.Metadata {
    private static final Playback.PlaybackTextPart[] EMPTY_PARAGRAPHS =
            new Playback.PlaybackTextPart[0];

    private final String mLanguageCode;
    private final String mCanonicalUrl;
    private final PlaybackMode mPlaybackMode;
    private String mTitle = "";
    private String mPublisher = "";

    NativeMetadata(
            @Nullable String languageCode,
            @Nullable String canonicalUrl,
            @Nullable PlaybackMode playbackMode) {
        mLanguageCode = languageCode != null ? languageCode : "";
        mCanonicalUrl = canonicalUrl != null ? canonicalUrl : "";
        mPlaybackMode = playbackMode != null ? playbackMode : PlaybackMode.CLASSIC;
    }

    void setTitle(@Nullable String title) {
        mTitle = title != null ? title : "";
    }

    void setPublisher(@Nullable String publisher) {
        mPublisher = publisher != null ? publisher : "";
    }

    @Override
    public String languageCode() {
        return mLanguageCode;
    }

    @Override
    public String title() {
        return mTitle;
    }

    @Override
    public String publisher() {
        return mPublisher;
    }

    @Override
    public String author() {
        // Author attribution is not currently displayed in the player UI.
        return "";
    }

    @Override
    public String fullText() {
        // TODO(b/537855631): Populate fullText from native service when highlighting and
        // tap-to-seek are supported.
        return "";
    }

    @Override
    public Playback.PlaybackTextPart[] paragraphs() {
        // TODO(b/537855631): Populate paragraph text parts from native service when granular
        // highlighting is supported.
        return EMPTY_PARAGRAPHS;
    }

    @Override
    public long estimatedDurationSeconds() {
        // Not used by the player UI. Exact audio duration is reported dynamically by
        // ReadAloudService via onPlaybackProgressUpdated.
        return 0;
    }

    @Override
    public String canonicalUrl() {
        return mCanonicalUrl;
    }

    @Override
    public PlaybackMode playbackMode() {
        return mPlaybackMode;
    }
}
