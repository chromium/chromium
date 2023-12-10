// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Interface for receiving updates on playback state during playback. */
public interface PlaybackListener {
    /** Playback state. */
    @IntDef({
        State.UNKNOWN,
        State.ERROR,
        State.BUFFERING,
        State.PAUSED,
        State.PLAYING,
        State.STOPPED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        /** Unknown. */
        int UNKNOWN = 0;

        /** Error. */
        int ERROR = 1;

        /** Buffering (audio isn't playing). */
        int BUFFERING = 3;

        /** Paused. */
        int PAUSED = 4;

        /** Playing. */
        int PLAYING = 5;

        /** Stopped; represents end of playback. */
        int STOPPED = 6;
    }

    /** Information about playback. */
    interface PlaybackData {
        /** Current playback state. */
        @State
        int state();

        /** Current paragraph index. */
        int paragraphIndex();

        /** Audio position in nanoseconds relative to beginning of paragraph. */
        long positionInParagraphNanos();

        /** Duration of the current paragraph in nanoseconds. */
        long paragraphDurationNanos();

        /** Absolute audio position in nanoseconds. */
        long absolutePositionNanos();

        /** Total audio duration in nanoseconds. */
        long totalDurationNanos();
    }

    /**
     * Called when playback data changes.
     * @param data Updated playback data.
     */
    default void onPlaybackDataChanged(PlaybackData data) {}

    /**
     * Indicates that an error occurred in playback.
     * @param error Caught exception.
     */
    default void onError(Exception error) {}

    /** Information needed for highlighting phrases. */
    interface PhraseTiming {
        /** The start offset of the phrase, relative to the full text. */
        int absoluteStartIndex();

        /** The end offset of the phrase, relative to the full text. */
        int absoluteEndIndex();

        /**
         * The start time offset of the phrase, relative to the start of the audio in the specific
         * paragraph.
         */
        long startTimeMillis();

        /**
         * The end time offset of the phrase, relative to the start of the audio in the specific
         * paragraph.
         */
        long endTimeMillis();

        /**
         * The start offset of the phrase, relative to the paragraph text which was used for TTS.
         */
        int relativeStartIndex();

        /** The end offset of the phrase, relative to the paragraph text which was used for TTS. */
        int relativeEndIndex();
    }

    /**
     * Called when the next phrase is reached in playback.
     * @param phraseTiming Phrase indices and times.
     */
    default void onPhraseChanged(PhraseTiming phraseTiming) {}

    /**
     * Called when the next phrase is reached in playback.
     * TODO remove when new version is implemented.
     * @param phraseTiming Serialized PhraseTiming proto message.
     */
    default void onPhraseChanged(byte[] phraseTiming) {}

    /**
     * Called when a paragraph ends in playback.
     * @param paragraphIndex Index of paragraph.
     */
    default void onParagraphChanged(int paragraphIndex) {}
}
