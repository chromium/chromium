// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud.external;

/** Interface for receiving updates on playback state during playback. */
public interface PlaybackListener {
    /**
     * Called when playback data changes.
     * @param readAloudPlaybackData Serialized ReadAloudPlaybackData proto message.
     */
    default void onPlaybackDataChanged(byte[] readAloudPlaybackData) {}

    /**
     * Indicates that an error occurred in playback.
     * @param error Caught exception.
     */
    default void onError(Exception error) {}

    /**
     * Called when the next phrase is reached in playback.
     * @param phraseTiming Serialized PhraseTiming proto message.
     */
    default void onPhraseChanged(byte[] phraseTiming) {}

    /**
     * Called when a paragraph ends in playback.
     * @param paragraphIndex Index of paragraph.
     */
    default void onParagraphChanged(int paragraphIndex) {}
}
