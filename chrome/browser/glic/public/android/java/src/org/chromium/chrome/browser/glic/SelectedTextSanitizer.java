// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.icu.lang.UCharacter;
import android.icu.lang.UCharacterDirection;
import android.icu.lang.UProperty;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Set;

/**
 * Removes characters that render as nothing from text selected on a web page, so that the page
 * cannot use them to hide text in the Ask Gemini prompt.
 */
@NullMarked
final class SelectedTextSanitizer {
    private static final int NONE = -1;
    private static final int ASCII_LIMIT = 0x80;
    private static final int VIRAMA_COMBINING_CLASS = 9;
    private static final int ARABIC_LETTER_MARK = 0x061C;
    private static final int ZERO_WIDTH_NON_JOINER = 0x200C;
    private static final int ZERO_WIDTH_JOINER = 0x200D;
    private static final int LEFT_TO_RIGHT_MARK = 0x200E;
    private static final int RIGHT_TO_LEFT_MARK = 0x200F;
    private static final int COMBINING_ENCLOSING_KEYCAP = 0x20E3;
    private static final int TEXT_VARIATION_SELECTOR = 0xFE0E;
    private static final int EMOJI_VARIATION_SELECTOR = 0xFE0F;
    private static final int INTERLINEAR_ANNOTATION_ANCHOR = 0xFFF9;
    private static final int INTERLINEAR_ANNOTATION_TERMINATOR = 0xFFFB;
    private static final int OBJECT_REPLACEMENT_CHARACTER = 0xFFFC;
    private static final int WAVING_BLACK_FLAG = 0x1F3F4;
    private static final int TAG_BASE = 0xE0000;
    private static final int TAG_SPACE = 0xE0020;
    private static final int TAG_TILDE = 0xE007E;
    private static final int TAG_CANCEL = 0xE007F;
    private static final String ELLIPSIS = "…";

    /** Tag letters of the only RGI subdivision flags (England, Scotland, Wales). */
    private static final Set<String> RGI_SUBDIVISION_FLAG_TAGS = Set.of("gbeng", "gbsct", "gbwls");

    private static final int MAX_SUBDIVISION_TAG_LENGTH = 5;

    private SelectedTextSanitizer() {}

    /**
     * Strips invisible characters, trims, and caps the length of a text selection.
     *
     * @param maxLength Maximum number of UTF-16 units kept before an ellipsis is appended, or a
     *     non-positive value for no cap.
     * @return The sanitized text, or an empty string if nothing remains.
     */
    static String sanitize(@Nullable String text, int maxLength) {
        if (TextUtils.isEmpty(text)) {
            return "";
        }
        // Invisible characters are stripped before the cap so they cannot consume the budget.
        String cleaned = stripInvisibleCodePoints(text).trim();
        if (maxLength <= 0 || cleaned.length() <= maxLength) {
            return cleaned;
        }
        int end =
                Character.isHighSurrogate(cleaned.charAt(maxLength - 1))
                        ? maxLength - 1
                        : maxLength;
        // Strip again so a joiner or tag sequence cut at the boundary is not left dangling.
        String head = stripInvisibleCodePoints(cleaned.substring(0, end)).trim();
        return head.isEmpty() ? "" : head + ELLIPSIS;
    }

    /**
     * Removes invisible characters, keeping default-ignorable ones only as part of a visible
     * sequence: joiners inside emoji or cursive/Indic words, single bidi marks in text with
     * right-to-left letters, presentation selectors after an emoji, and the tag sequences of RGI
     * subdivision flags.
     */
    private static String stripInvisibleCodePoints(String text) {
        boolean hasRtlLetters = containsRtlLetter(text);
        StringBuilder out = new StringBuilder(text.length());
        int lastKept = NONE;
        int lastKeptNonTransparent = NONE;
        int i = 0;
        while (i < text.length()) {
            int cp = text.codePointAt(i);
            if (isTag(cp)) {
                int flagEnd =
                        lastKept == WAVING_BLACK_FLAG ? findSubdivisionFlagEnd(text, i) : NONE;
                if (flagEnd != NONE) {
                    out.append(text, i, flagEnd);
                    lastKept = TAG_CANCEL;
                    i = flagEnd;
                } else {
                    i = skipTagRun(text, i);
                }
                continue;
            }
            int nextIndex = i + Character.charCount(cp);
            if (shouldKeepCodePoint(
                    lastKept, lastKeptNonTransparent, cp, text, nextIndex, hasRtlLetters)) {
                out.appendCodePoint(cp);
                lastKept = cp;
                if (!isTransparentToJoining(cp)) {
                    lastKeptNonTransparent = cp;
                }
            }
            i = nextIndex;
        }
        return out.toString();
    }

    private static boolean shouldKeepCodePoint(
            int lastKept,
            int lastKeptNonTransparent,
            int cp,
            String text,
            int nextIndex,
            boolean hasRtlLetters) {
        if (cp == '\t' || cp == '\n' || cp == '\r') {
            return true;
        }
        int type = Character.getType(cp);
        if (type == Character.CONTROL
                || type == Character.SURROGATE
                || cp == OBJECT_REPLACEMENT_CHARACTER
                || (cp >= INTERLINEAR_ANNOTATION_ANCHOR
                        && cp <= INTERLINEAR_ANNOTATION_TERMINATOR)) {
            return false;
        }
        if (!hasProperty(cp, UProperty.DEFAULT_IGNORABLE_CODE_POINT)) {
            return true;
        }
        int next = codePointAtOrNone(text, nextIndex);
        switch (cp) {
            case ZERO_WIDTH_JOINER:
                return (isEmojiJoinerPrefix(lastKept)
                                && hasProperty(next, UProperty.EXTENDED_PICTOGRAPHIC))
                        || isShapingJoinerContext(
                                cp, lastKept, lastKeptNonTransparent, text, nextIndex);
            case ZERO_WIDTH_NON_JOINER:
                return isShapingJoinerContext(
                        cp, lastKept, lastKeptNonTransparent, text, nextIndex);
            case ARABIC_LETTER_MARK:
            case LEFT_TO_RIGHT_MARK:
            case RIGHT_TO_LEFT_MARK:
                // A mark can change the order of digits and punctuation around right-to-left
                // letters.
                return hasRtlLetters && !isBidiMark(lastKept);
            case TEXT_VARIATION_SELECTOR:
            case EMOJI_VARIATION_SELECTOR:
                // VS15 and VS16 are not emoji, so a selector right after another selector is
                // dropped. Devices with older emoji data drop the selector after newer emoji; only
                // the presentation style is lost.
                if (!hasProperty(lastKept, UProperty.EMOJI)) {
                    return false;
                }
                // The only ASCII emoji are digits, '#' and '*', which are RGI emoji only in
                // keycap sequences.
                return lastKept >= ASCII_LIMIT || next == COMBINING_ENCLOSING_KEYCAP;
            default:
                return false;
        }
    }

    private static boolean isEmojiJoinerPrefix(int cp) {
        return cp == EMOJI_VARIATION_SELECTOR
                || hasProperty(cp, UProperty.EXTENDED_PICTOGRAPHIC)
                || hasProperty(cp, UProperty.EMOJI_MODIFIER);
    }

    /**
     * Whether {@code joiner} (ZWJ or ZWNJ) is in a shaping context: after a virama (Indic half
     * forms, conjuncts and chillus), or next to a letter that cursively joins toward it (Arabic,
     * Syriac, N'Ko, Mongolian, ...), skipping harakat and other characters that are transparent to
     * joining. A ZWNJ needs such a letter on each side. A ZWJ needs only one, before or after it,
     * as it selects that letter's joining form, or a virama right after it (Sinhala touching
     * letters). Ligature control in other scripts, e.g. a ZWNJ between "f" and "l", is not kept.
     */
    private static boolean isShapingJoinerContext(
            int joiner, int lastKept, int lastKeptNonTransparent, String text, int nextIndex) {
        // No letter needs to follow: a word-final virama + ZWJ can spell a chillu or khanda ta.
        if (isVirama(lastKept)) {
            return true;
        }
        if (isJoiner(lastKeptNonTransparent)) {
            return false;
        }
        boolean prevJoins = canJoinFollowing(lastKeptNonTransparent);
        if (joiner == ZERO_WIDTH_JOINER) {
            return prevJoins
                    || isVirama(codePointAtOrNone(text, nextIndex))
                    || canJoinPreceding(findNextNonTransparent(text, nextIndex));
        }
        return prevJoins && canJoinPreceding(findNextNonTransparent(text, nextIndex));
    }

    private static boolean isVirama(int cp) {
        return cp != NONE && UCharacter.getCombiningClass(cp) == VIRAMA_COMBINING_CLASS;
    }

    private static boolean isJoiner(int cp) {
        return cp == ZERO_WIDTH_JOINER || cp == ZERO_WIDTH_NON_JOINER;
    }

    private static boolean canJoinFollowing(int cp) {
        int joiningType = getVisibleJoiningType(cp);
        return joiningType == UCharacter.JoiningType.DUAL_JOINING
                || joiningType == UCharacter.JoiningType.LEFT_JOINING
                || joiningType == UCharacter.JoiningType.JOIN_CAUSING;
    }

    private static boolean canJoinPreceding(int cp) {
        int joiningType = getVisibleJoiningType(cp);
        return joiningType == UCharacter.JoiningType.DUAL_JOINING
                || joiningType == UCharacter.JoiningType.RIGHT_JOINING
                || joiningType == UCharacter.JoiningType.JOIN_CAUSING;
    }

    private static int getVisibleJoiningType(int cp) {
        // ZWJ is join-causing, so without this the first joiner of a run would treat the next ZWJ
        // as a joining letter and could be kept.
        if (cp == NONE || hasProperty(cp, UProperty.DEFAULT_IGNORABLE_CODE_POINT)) {
            return UCharacter.JoiningType.NON_JOINING;
        }
        return UCharacter.getIntPropertyValue(cp, UProperty.JOINING_TYPE);
    }

    private static boolean isTransparentToJoining(int cp) {
        return UCharacter.getIntPropertyValue(cp, UProperty.JOINING_TYPE)
                == UCharacter.JoiningType.TRANSPARENT;
    }

    private static int codePointAtOrNone(String text, int index) {
        return index < text.length() ? text.codePointAt(index) : NONE;
    }

    private static int findNextNonTransparent(String text, int start) {
        int i = start;
        while (i < text.length()) {
            int cp = text.codePointAt(i);
            if (!isTransparentToJoining(cp)) {
                return cp;
            }
            i += Character.charCount(cp);
        }
        return NONE;
    }

    private static boolean isBidiMark(int cp) {
        return cp == ARABIC_LETTER_MARK || cp == LEFT_TO_RIGHT_MARK || cp == RIGHT_TO_LEFT_MARK;
    }

    private static boolean containsRtlLetter(String text) {
        int i = 0;
        while (i < text.length()) {
            int cp = text.codePointAt(i);
            int direction = UCharacter.getDirection(cp);
            // RLM and ALM have right-to-left direction too.
            if ((direction == UCharacterDirection.RIGHT_TO_LEFT
                            || direction == UCharacterDirection.RIGHT_TO_LEFT_ARABIC)
                    && !isBidiMark(cp)) {
                return true;
            }
            i += Character.charCount(cp);
        }
        return false;
    }

    private static boolean hasProperty(int cp, int property) {
        return cp != NONE && UCharacter.hasBinaryProperty(cp, property);
    }

    private static boolean isTag(int cp) {
        return cp >= TAG_BASE && cp <= TAG_CANCEL;
    }

    private static int skipTagRun(String text, int start) {
        int i = start;
        while (i < text.length() && isTag(text.codePointAt(i))) {
            i += Character.charCount(text.codePointAt(i));
        }
        return i;
    }

    private static int findSubdivisionFlagEnd(String text, int start) {
        StringBuilder tagLetters = new StringBuilder();
        int i = start;
        while (i < text.length() && tagLetters.length() <= MAX_SUBDIVISION_TAG_LENGTH) {
            int cp = text.codePointAt(i);
            i += Character.charCount(cp);
            if (cp == TAG_CANCEL) {
                return RGI_SUBDIVISION_FLAG_TAGS.contains(tagLetters.toString()) ? i : NONE;
            }
            if (cp < TAG_SPACE || cp > TAG_TILDE) {
                return NONE;
            }
            tagLetters.append((char) (cp - TAG_BASE));
        }
        return NONE;
    }
}
