// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextType;
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
    private String mFullText = "";
    private Playback.PlaybackTextPart[] mParagraphs = EMPTY_PARAGRAPHS;

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

    void setFullText(@Nullable String fullText) {
        mFullText = fullText != null ? fullText : "";
    }

    void setParagraphs(Playback.PlaybackTextPart @Nullable [] paragraphs) {
        mParagraphs = paragraphs != null ? paragraphs : EMPTY_PARAGRAPHS;
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
        return mFullText;
    }

    @Override
    public Playback.PlaybackTextPart[] paragraphs() {
        return mParagraphs;
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

    /** Concrete implementation of {@link Playback.PlaybackTextPart} for native playback. */
    static class TextPart implements Playback.PlaybackTextPart {
        private final int mParagraphIndex;
        private final int mOffset;
        private final int mLength;
        @PlaybackTextType private final int mType;

        TextPart(int paragraphIndex, int offset, int length, @PlaybackTextType int type) {
            mParagraphIndex = paragraphIndex;
            mOffset = offset;
            mLength = length;
            mType = type;
        }

        @Override
        public int getParagraphIndex() {
            return mParagraphIndex;
        }

        @Override
        public int getOffset() {
            return mOffset;
        }

        @Override
        public int getLength() {
            return mLength;
        }

        @Override
        public @PlaybackTextType int getType() {
            return mType;
        }
    }
}
