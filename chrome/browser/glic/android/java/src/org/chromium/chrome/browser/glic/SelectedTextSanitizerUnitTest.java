// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SelectedTextSanitizer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SelectedTextSanitizerUnitTest {
    private static final int MAX = 1000;
    private static final String BLACK_FLAG = "\uD83C\uDFF4";
    private static final String TAG_CANCEL = "\uDB40\uDC7F";

    private static String sanitize(String text) {
        return SelectedTextSanitizer.sanitize(text, MAX);
    }

    @Test
    public void testNullAndEmpty() {
        assertEquals("", SelectedTextSanitizer.sanitize(null, MAX));
        assertEquals("", sanitize(""));
    }

    @Test
    public void testStripsControlAndObjectReplacementKeepingWhitespace() {
        assertEquals("Hello\n\tworld!", sanitize("  Hello\uFFFC\u0000\u0007\u0085\n\tworld!  "));
    }

    @Test
    public void testStripsDefaultIgnorableCharacters() {
        // ZWSP, LRM, RLM, ALM, word joiner, invisible separator, BOM, soft hyphen, Hangul filler,
        // bidi override and isolate, and an interlinear annotation anchor.
        assertEquals(
                "Hello world",
                sanitize(
                        "He\u200Bl\u200El\u200Fo\u061C \u2060w\u2063o\uFEFFr\u00ADl\u3164d"
                                + "\u202E\u2066\uFFF9"));
    }

    @Test
    public void testStripsVariationSelectorsAfterNonEmoji() {
        // VS1, VS16 and VS17 after ASCII letters, and a VS at the start of the text.
        assertEquals("abc", sanitize("\uFE0Fa\uFE00b\uFE0Fc\uDB40\uDD00"));
        // A letter followed by VS16 and a keycap.
        assertEquals("a\u20E3", sanitize("a\uFE0F\u20E3"));
    }

    @Test
    public void testStripsVariationSelectorAfterDigitUnlessKeycap() {
        assertEquals("12", sanitize("1\uFE0F2\uFE0E"));
        assertEquals("1\uFE0F\u20E3", sanitize("1\uFE0F\u20E3"));
        assertEquals("#\uFE0F\u20E3*\uFE0F\u20E3", sanitize("#\uFE0F\u20E3*\uFE0F\u20E3"));
    }

    @Test
    public void testKeepsOnlyFirstVariationSelectorAfterEmoji() {
        // Heavy black heart followed by runs of variation selectors.
        assertEquals("\u2764\uFE0F", sanitize("\u2764\uFE0F\uFE0F\uFE0E"));
        assertEquals("\u2764\uFE0E", sanitize("\u2764\uFE0E\uFE0F"));
    }

    @Test
    public void testStripsTagsOutsideSubdivisionFlags() {
        assertEquals("hi", sanitize("h\uDB40\uDC61i\uDB40\uDC62" + TAG_CANCEL));
        // Tags spelling "hi" after a black flag are not an RGI flag, so only the flag remains.
        assertEquals(BLACK_FLAG, sanitize(BLACK_FLAG + "\uDB40\uDC68\uDB40\uDC69" + TAG_CANCEL));
    }

    @Test(timeout = 5000)
    public void testLongTagRunAfterFlagIsLinear() {
        assertEquals(BLACK_FLAG + "x", sanitize(BLACK_FLAG + "\uDB40\uDC61".repeat(100_000) + "x"));
    }

    @Test
    public void testPreservesEmojiSequences() {
        String family = "\uD83D\uDC68\u200D\uD83D\uDC69\u200D\uD83D\uDC67";
        String technologistWithSkinTone = "\uD83D\uDC69\uD83C\uDFFD\u200D\uD83D\uDCBB";
        String rainbowFlag = "\uD83C\uDFF3\uFE0F\u200D\uD83C\uDF08";
        String redHeart = "\u2764\uFE0F";
        String scotlandFlag =
                BLACK_FLAG
                        + "\uDB40\uDC67\uDB40\uDC62\uDB40\uDC73\uDB40\uDC63\uDB40\uDC74"
                        + TAG_CANCEL;
        String emoji = family + technologistWithSkinTone + rainbowFlag + redHeart + scotlandFlag;
        assertEquals(emoji, sanitize(emoji));
    }

    @Test
    public void testStripsJoinersWithoutShapingEffect() {
        assertEquals("secret", sanitize("se\u200Dcr\u200Ce\u200D\u200Dt"));
        assertEquals("中文字", sanitize("中\u200C文\u200D字"));
        assertEquals("привет", sanitize("при\u200Dвет"));
        assertEquals("\uD83D\uDE00", sanitize("\u200D\uD83D\uDE00\u200D"));
    }

    @Test
    public void testPreservesShapingJoiners() {
        // Persian ZWNJ between dual-joining letters; Devanagari virama + ZWJ half form.
        String persian = "\u0645\u06CC\u200C\u062E\u0648\u0627\u0647\u0645";
        String devanagari = "\u0915\u094D\u200D\u0937";
        assertEquals(persian, sanitize(persian));
        assertEquals(devanagari, sanitize(devanagari));
    }

    @Test
    public void testPreservesZeroWidthNonJoinerNextToMarksAndTatweel() {
        // A sukun before the ZWNJ, an RLM after it, and a tatweel after it.
        String sukun = "\u062F\u0631\u062E\u062A\u0652\u200C\u0647\u0627";
        String rlm = "\u0628\u200C\u200F\u0628";
        String tatweel = "\u0628\u200C\u0640\u0647";
        assertEquals(sukun, sanitize(sukun));
        assertEquals(rlm, sanitize(rlm));
        assertEquals(tatweel, sanitize(tatweel));
    }

    @Test
    public void testPreservesZeroWidthJoinerNextToOneJoiningLetter() {
        // Medial AIN, initial HEH and final ALEF on their own, and final BEH after a Latin letter.
        String joiningForms = "\u200D\u0639\u200D \u0647\u200D \u200D\u0627";
        String afterLatin = "a\u200D\u0628";
        assertEquals(joiningForms, sanitize(joiningForms));
        assertEquals(afterLatin, sanitize(afterLatin));
    }

    @Test
    public void testStripsJoinersNextToLettersThatDoNotJoinThroughThem() {
        // DAL only joins to the letter before it, and a ZWNJ needs a joining letter on each side.
        assertEquals("\u0628\u062F", sanitize("\u0628\u062F\u200D"));
        assertEquals("\u062F\u0628", sanitize("\u062F\u200C\u0628"));
        assertEquals("\u0628 \u0627", sanitize("\u0628\u200C \u0627"));
        assertEquals("\u0D9A\u0DCA\u0DC0", sanitize("\u0D9A\u200C\u0DCA\u0DC0"));
    }

    @Test
    public void testPreservesJoinerAfterWordFinalVirama() {
        // Malayalam chillu N and Bengali khanda ta spelled with virama + ZWJ.
        String malayalam = "\u0D05\u0D35\u0D28\u0D4D\u200D \u0D35\u0D28\u0D4D\u0D28\u0D41";
        String bengali = "\u09B9\u09A0\u09BE\u09A4\u09CD\u200D";
        assertEquals(malayalam, sanitize(malayalam));
        assertEquals(bengali, sanitize(bengali));
    }

    @Test
    public void testPreservesZeroWidthJoinerBeforeVirama() {
        // Sinhala touching letters are spelled consonant + ZWJ + al-lakuna + consonant.
        String touching = "\u0D9A\u200D\u0DCA\u0DC0";
        assertEquals(touching, sanitize(touching));
    }

    @Test
    public void testKeepsOneJoinerBetweenLetters() {
        assertEquals("\u0628\u200C\u0628", sanitize("\u0628\u200C\u200C\u0628"));
        assertEquals("\u0628\u200D\u0628", sanitize("\u0628\u200D\u200D\u200D\u0628"));
        assertEquals("\u0628\u200D\u064E\u0628", sanitize("\u0628\u200D\u064E\u200D\u0628"));
        assertEquals("\u0915\u094D\u200D\u0937", sanitize("\u0915\u094D\u200D\u200D\u0937"));
        assertEquals("\u0D9A\u200D\u0DCA\u0DC0", sanitize("\u0D9A\u200D\u200D\u0DCA\u0DC0"));
    }

    @Test(timeout = 5000)
    public void testLongMarkRunAfterJoinerIsLinear() {
        String text = "\u0628\u200C" + "\u064E".repeat(100_000) + "\u0647";
        assertEquals(text, SelectedTextSanitizer.sanitize(text, 0));
    }

    @Test(timeout = 5000)
    public void testLongJoinerRunIsLinear() {
        String text = "\u0628" + "\u200C".repeat(100_000) + "\u0647";
        assertEquals("\u0628\u200C\u0647", sanitize(text));
    }

    @Test
    public void testKeepsSingleBidiMarksInRightToLeftText() {
        String rlmBeforeLatin =
                "\u200FGoogle \u0634\u0631\u0643\u0629 \u0623\u0645\u0631\u064A\u0643\u064A\u0629";
        String almAndLrm = "\u061C\u0661-\u0662 a\u200Eb \u0643\u0644";
        assertEquals(rlmBeforeLatin, sanitize(rlmBeforeLatin));
        assertEquals(almAndLrm, sanitize(almAndLrm));
        assertEquals("\u0628\u200F\u062A", sanitize("\u0628\u200F\u200F\u200F\u062A"));
    }

    @Test
    public void testStripsBidiOverridesAndIsolatesInRightToLeftText() {
        assertEquals("\u0628abcd\u062A", sanitize("\u0628\u202Eabc\u202C\u2067d\u2069\u062A"));
    }

    @Test
    public void testStripsLoneSurrogates() {
        assertEquals("ab", sanitize("a\uD83Db\uDE00"));
    }

    @Test
    public void testDoesNotTruncateAtExactLimit() {
        String exact = "a".repeat(MAX);
        assertEquals(exact, sanitize(exact));
    }

    @Test
    public void testTruncatesAfterStrippingInvisibleCharacters() {
        String text = "\u200B".repeat(MAX) + "a".repeat(MAX + 5);
        assertEquals("a".repeat(MAX) + "…", sanitize(text));
    }

    @Test
    public void testTruncatesOnCodePointBoundary() {
        String text = "a".repeat(MAX - 1) + "\uD83D\uDE00b";
        assertEquals("a".repeat(MAX - 1) + "…", sanitize(text));
    }

    @Test
    public void testTruncationDropsDanglingJoiner() {
        // The cut lands right after the ZWJ inside a family emoji.
        String man = "\uD83D\uDC68";
        String text = "a".repeat(MAX - 3) + man + "\u200D\uD83D\uDC69";
        assertEquals("a".repeat(MAX - 3) + man + "…", sanitize(text));
    }

    @Test
    public void testTruncationNeverReturnsBareEllipsis() {
        assertEquals("", SelectedTextSanitizer.sanitize("\uD83D\uDE00\uD83D\uDE00", 1));
    }

    @Test
    public void testNonPositiveMaxLengthDisablesCap() {
        String text = "a".repeat(MAX + 5);
        assertEquals(text, SelectedTextSanitizer.sanitize(text, 0));
    }
}
