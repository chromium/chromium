// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextPart;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextType;

import java.util.Arrays;
import java.util.Comparator;

/**
 * Handles the tap to seek feature for Read Aloud.
 *
 * <p>Uses the following heuristics to search for the selected page content in the playback's
 * distilled text:
 *
 * <ul>
 *   <li>selected word and +-15 characters on either side, without parenthesized text
 *   <li>selected word and 15 characters before it, without parenthesized text
 *   <li>selected word and 15 characters after it, without parenthesized text
 * </ul>
 *
 * The full text and content has had all whitespaces replaced with a single space to remove new
 * lines and duplicate white spaces.
 */
public class TapToSeekHandler {
    /**
     * Finds the first substring match of content in the playback's full text and seeks playback to
     * the selected word. If no match is found, this doesn't seek. Different substrings of content
     * are used to search since there can be discrepancies between the tab's content and the
     * playback's distilled text.
     *
     * @param content Selected word and surrounding content
     * @param beginOffset index of where the selected word starts within the content
     * @param endOffset index of where the selected word ends within the content
     * @param playback the playback that will be seeked
     * @param playing whether the playback is in the play state when the tap to seek request was
     *     started
     */
    public static void tapToSeek(
            String content, int beginOffset, int endOffset, Playback playback, boolean playing) {
        if (content == null || content.isEmpty()) {
            return;
        }
        char[] fullText = playback.getMetadata().fullText().toCharArray();
        // Set the needle to the word +- 15 characters on either side.
        int substringStartIndex = Math.max(0, beginOffset - 15);
        int substringEndIndex = Math.min(content.length() - 1, endOffset + 15);
        if (substringEndIndex <= 0) {
            return;
        }
        String needle =
                content.substring(substringStartIndex, substringEndIndex)
                        .replaceAll(
                                "[\\[\\(][\\s\\S]*?[\\]\\)]",
                                "") // removes any () and [] and inner content
                        .replaceAll("\\s+", " "); // replaces any white-spaces with a space.
        int found = BoyerMoore.indexOf(fullText, needle.toCharArray());
        if (found > 0) {
            maybeTapToSeek(found + beginOffset - substringStartIndex, playback, playing);
        } else {
            // Last needle not matched, try with the word and -15 characters.
            substringStartIndex = Math.max(0, beginOffset - 15);
            substringEndIndex = endOffset;
            needle =
                    content.substring(substringStartIndex, substringEndIndex)
                            .trim()
                            .replaceAll(
                                    "[\\[\\(][\\s\\S]*?[\\]\\)]",
                                    "") // removes any () and [] and inner content
                            .replaceAll("\\s+", " "); // replaces any white-spaces with a space.
            found = BoyerMoore.indexOf(fullText, needle.toCharArray());
            if (found > 0) {
                maybeTapToSeek(found + beginOffset - substringStartIndex, playback, playing);
            } else {
                // Last needle not matched, try with the word and +15 characters.
                substringStartIndex = beginOffset;
                substringEndIndex = Math.min(content.length() - 1, endOffset + 15);
                needle =
                        content.substring(substringStartIndex, substringEndIndex)
                                .trim()
                                .replaceAll(
                                        "[\\[\\(][\\s\\S]*?[\\]\\)]",
                                        "") // removes any () and [] and inner content
                                .replaceAll("\\s+", " "); // replaces any white-space with a space.
                found = BoyerMoore.indexOf(fullText, needle.toCharArray());
                if (found > 0) {
                    maybeTapToSeek(found, playback, playing);
                } else {
                    // TODO: b/325654229 Improve heuristics with more substrings to match with.
                    ReadAloudMetrics.recordHasTapToSeekFoundMatch(false);
                }
            }
        }
    }

    private static Comparator<PlaybackTextPart> sComparator =
            new Comparator<>() {
                @Override
                public int compare(PlaybackTextPart a, PlaybackTextPart b) {
                    return Integer.compare(a.getOffset(), b.getOffset());
                }
            };

    /**
     * If the insertion point is found in the playbacks paragraphs, seek the playback to the word
     * and play.
     *
     * @param index index of the selected word found in the full text using Boyer moore
     * @param content selected word and surrounding content
     * @param playback playback that will be seeked
     */
    private static void maybeTapToSeek(int index, Playback playback, boolean playing) {
        int paragraphIndex = findParagraph(playback.getMetadata().paragraphs(), index);
        int wordIndex = findWord(playback.getMetadata().paragraphs()[paragraphIndex], index);
        if (wordIndex < 0) {
            ReadAloudMetrics.recordHasTapToSeekFoundMatch(false);
        } else {
            playback.seekToWord(paragraphIndex, wordIndex);
            if (playing) {
                playback.play();
            } else {
                playback.pause();
            }
            ReadAloudMetrics.recordHasTapToSeekFoundMatch(true);
        }
    }

    private static int findParagraph(PlaybackTextPart[] paragraphs, int offset) {
        if (offset > paragraphs[paragraphs.length - 1].getOffset()) {
            return paragraphs.length - 1;
        }

        PlaybackTextPart p =
                new PlaybackTextPart() {
                    @Override
                    public int getOffset() {
                        return offset;
                    }

                    @Override
                    public int getType() {
                        return PlaybackTextType.TEXT_TYPE_UNSPECIFIED;
                    }

                    @Override
                    public int getParagraphIndex() {
                        return -1;
                    }

                    @Override
                    public int getLength() {
                        return -1;
                    }
                };

        int i = Arrays.binarySearch(paragraphs, p, sComparator);
        if (i >= 0) {
            return i;
        }
        int insertionPoint = -i - 1;
        return insertionPoint - 1;
    }

    private static int findWord(PlaybackTextPart paragraph, int offset) {
        if (offset < paragraph.getOffset()) {
            return -1;
        } else {
            return offset - paragraph.getOffset();
        }
    }
}
