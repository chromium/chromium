// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

/** Represents a single audio playback session. */
public interface Playback {
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

    /**
     * Pause the media. If already paused, does nothing.
     */
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
}
