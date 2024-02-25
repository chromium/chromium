// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Represents a single audio playback session. */
public interface Playback {
    /** Metadata describing the content of the playback */
    interface Metadata {
        String languageCode();

        String title();

        String publisher();

        String author();

        String fullText();

        PlaybackTextPart[] paragraphs();

        long estimatedDurationSeconds();

        String canonicalUrl();
    }

    /**
     * Represents a single text part.
     * This might be a paragraph, a sentence or any semantic unit of the article.
     */
    interface PlaybackTextPart {
        // The index of the text part's paragraph in the full article.
        int getParagraphIndex();

        // The offset of the text part in the full text.
        int getOffset();

        // The length of the text part, in characters.
        int getLength();

        @PlaybackTextType
        int getType();
    }

    /** Type of a text portion. */
    @IntDef({
        PlaybackTextType.TEXT_TYPE_UNSPECIFIED,
        PlaybackTextType.TEXT_TYPE_NORMAL,
        PlaybackTextType.TEXT_TYPE_TITLE,
        PlaybackTextType.TEXT_TYPE_PUBLISHER_AND_AUTHOR,
        PlaybackTextType.TEXT_TYPE_PUBLISHER,
        PlaybackTextType.TEXT_TYPE_AUTHOR
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PlaybackTextType {
        // Unspecified.
        int TEXT_TYPE_UNSPECIFIED = 0;
        // Normal text (without a particular role).
        int TEXT_TYPE_NORMAL = 1;
        // The title of a document.
        int TEXT_TYPE_TITLE = 2;
        // The publisher and author part of the document.
        int TEXT_TYPE_PUBLISHER_AND_AUTHOR = 3;
        // The publisher or the document.
        int TEXT_TYPE_PUBLISHER = 4;
        // The author of the document.
        int TEXT_TYPE_AUTHOR = 5;
    }

    /**
     * Returns the metadata represented by this playback.
     * @return Metadata with language, title, publisher, full text, etc.
     */
    default Metadata getMetadata() {
        return null;
    }

    /**
     * Add a listener to be called on playback events.
     * @param listener Listener.
     */
    default void addListener(PlaybackListener listener) {}

    /**
     * Remove a listener.
     * @param listener Listener.
     */
    default void removeListener(PlaybackListener listener) {}

    /**
     * Play the media. If already playing, does nothing. If paused, resume playback from paused
     * location.
     */
    default void play() {}

    /** Pause the media. If already paused, does nothing. */
    default void pause() {}

    /**
     * Seek playback relative to the current position.
     * @param seekDurationNanos Relative time by which to seek, nanoseconds. Rewind by passing a
     *         negative duration.
     */
    default void seekRelative(long seekDurationNanos) {}

    /**
     * Seek playback to an absolute position. Throws exception if duration is negative or past the
     * end.
     * @param absolutePositionNanos Seek target time relative to beginning of audio. Nanoseconds.
     */
    default void seek(long absolutePositionNanos) {}

    /**
     * Seek playback to the given paragraph and time offset within the paragraph. Throws an
     * exception if paragraph is out of range or offset is out of paragraph range.
     * @param paragraphIndex Index of paragraph to seek to.
     * @param offsetNanos Seek time relative to beginning of paragraph. Nanoseconds.
     */
    default void seekToParagraph(int paragraphIndex, long offsetNanos) {}

    /**
     * Seek playback to the given paragraph and word. Throws an exception if paragraph is out of
     * range or word is out of range in paragraph.
     * @param paragraphIndex Index of paragraph to seek to.
     * @param wordIndex Index of word to seek to.
     */
    default void seekToWord(int paragraphIndex, int wordIndex) {}

    /**
     * Set the playback rate.
     * @param rate Playback rate. Must be positive.
     */
    default void setRate(float rate) {}

    /**
     * Releases the playback and all associated resources (e.g audio notification
     * and media session).
     *
     * <p>
     * This method must be called when the playback is no longer needed.
     */
    default void release() {}
}
