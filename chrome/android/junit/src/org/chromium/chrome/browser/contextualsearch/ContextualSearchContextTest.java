// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;

/** Tests parts of the ContextualSearchContext class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextualSearchContextTest {
    private static final int INVALID = ContextualSearchContext.INVALID_OFFSET;
    private static final String SAMPLE_TEXT =
            "Now Barack Obama is not the best example.  And Clinton is ambiguous.";
    private static final String HOME_COUNTRY = "unused";
    private static final long NATIVE_PTR = 1;

    private ContextualSearchContext mContext;
    private boolean mDidSelectionChange;

    private class ContextualSearchContextForTest extends ContextualSearchContext {
        @Override
        void onSelectionChanged() {
            mDidSelectionChange = true;
        }
    }

    @Rule public JniMocker mocker = new JniMocker();

    @Mock private ContextualSearchContext.Natives mContextJniMock;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(ContextualSearchContextJni.TEST_HOOKS, mContextJniMock);
        when(mContextJniMock.init(any())).thenReturn(NATIVE_PTR);
        mDidSelectionChange = false;
        mContext = new ContextualSearchContextForTest();
    }

    private void setupLongpressOfBarack() {
        int barackStartOffset = "Now ".length();
        int barackEndOffset = "Now Barack".length() - barackStartOffset;
        mContext.setSurroundingText(SAMPLE_TEXT, barackStartOffset, barackEndOffset);
    }

    private void setupTapInBarack() {
        int barackBeforeROffset = "Now Ba".length();
        mContext.setSurroundingText(SAMPLE_TEXT, barackBeforeROffset, barackBeforeROffset);
    }

    private void simulateSelectWordAroundCaret(int startAdjust, int endAdjust) {
        mContext.onSelectionAdjusted(startAdjust, endAdjust);
    }

    private void simulateResolve(int startAdjust, int endAdjust) {
        mContext.onSelectionAdjusted(startAdjust, endAdjust);
    }

    private void setupResolvingTapInBarak() {
        setupTapInBarack();
        mContext.setResolveProperties(HOME_COUNTRY, true, "", "");
    }

    private void setupResolvingTapInObama() {
        int obamaBeforeMOffset = "Now Barack Oba".length();
        mContext.setSurroundingText(SAMPLE_TEXT, obamaBeforeMOffset, obamaBeforeMOffset);
        mContext.setResolveProperties(HOME_COUNTRY, true, "", "");
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testEmptyContext() {
        assertFalse(mContext.hasAnalyzedTap());
        assertFalse(mContext.hasValidTappedText());
        assertFalse(mContext.hasValidSelection());
        assertFalse(mContext.canResolve());
        assertNull(mContext.getWordTapped());
        assertEquals(INVALID, mContext.getTapOffsetWithinTappedWord());
        assertNull(mContext.getSurroundingText());
        assertEquals(INVALID, mContext.getSelectionStartOffset());
        assertEquals(INVALID, mContext.getSelectionEndOffset());
        assertNull(mContext.getEncoding());
        assertEquals("", mContext.getTextContentFollowingSelection());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testLongpressContext() {
        setupLongpressOfBarack();
        assertFalse(mContext.canResolve());
        assertFalse(mContext.hasAnalyzedTap());
        assertFalse(mContext.hasValidTappedText());
        assertTrue(mContext.hasValidSelection());
        assertNotNull(mContext.getSurroundingText());
        assertTrue(mContext.getSelectionStartOffset() >= 0);
        assertTrue(mContext.getSelectionEndOffset() >= 0);
        assertNotNull(mContext.getEncoding());
        assertTrue(mDidSelectionChange);
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testTapContext() {
        setupTapInBarack();
        assertFalse(mContext.canResolve());
        assertTrue(mContext.hasAnalyzedTap());
        assertTrue(mContext.hasValidTappedText());
        assertFalse(mContext.hasValidSelection());
        assertNotNull(mContext.getSurroundingText());
        assertTrue(mContext.getSelectionStartOffset() >= 0);
        assertTrue(mContext.getSelectionEndOffset() >= 0);
        assertNotNull(mContext.getEncoding());
        assertFalse(mDidSelectionChange);

        simulateSelectWordAroundCaret(-"Ba".length(), "rack".length());
        assertEquals(
                "Barack".length(),
                mContext.getSelectionEndOffset() - mContext.getSelectionStartOffset());
        assertTrue(mDidSelectionChange);
        assertTrue(mContext.hasValidSelection());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testResolvingTapContext() {
        setupResolvingTapInBarak();
        assertFalse(mContext.canResolve());

        simulateSelectWordAroundCaret(-"Ba".length(), "rack".length());
        assertTrue(mContext.canResolve());

        simulateResolve(0, " Obama".length());
        assertEquals(
                "Barack Obama".length(),
                mContext.getSelectionEndOffset() - mContext.getSelectionStartOffset());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisOfWord() {
        setupResolvingTapInBarak();
        assertEquals("Barack", mContext.getWordTapped());
        assertEquals("Ba".length(), mContext.getTapOffsetWithinTappedWord());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisAtStartOfText() {
        int startOffset = 0;
        mContext.setSurroundingText(SAMPLE_TEXT, startOffset, startOffset);
        // We can't recognize the first word because we need a space before it to do so.
        assertNull(mContext.getWordTapped());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisAtSecondWordOfText() {
        int secondWordOffset = "Now ".length();
        mContext.setSurroundingText(SAMPLE_TEXT, secondWordOffset, secondWordOffset);
        assertEquals("Barack", mContext.getWordTapped());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisAtEndOfText() {
        int endOffset = SAMPLE_TEXT.length();
        mContext.setSurroundingText(SAMPLE_TEXT, endOffset, endOffset);
        assertNull(mContext.getWordTapped());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisAtWordBeforeEndOfText() {
        int wordBeforeEndOffset = SAMPLE_TEXT.length() - "s ambiguous.".length();
        mContext.setSurroundingText(SAMPLE_TEXT, wordBeforeEndOffset, wordBeforeEndOffset);
        assertEquals("is", mContext.getWordTapped());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testRedundantLanguages() {
        // Most common to least common
        doNothing()
                .when(mContextJniMock)
                .setTranslationLanguages(anyLong(), eq(mContext), eq(""), eq(""), eq(""));
        mContext.setTranslationLanguages("en", "en", "en");
        doNothing()
                .when(mContextJniMock)
                .setTranslationLanguages(anyLong(), eq(mContext), eq(""), eq(""), eq(""));
        mContext.setTranslationLanguages("", "en", "en");
        doNothing()
                .when(mContextJniMock)
                .setTranslationLanguages(anyLong(), eq(mContext), eq("en"), eq("de"), eq(""));
        mContext.setTranslationLanguages("en", "de", "de");
        doNothing()
                .when(mContextJniMock)
                .setTranslationLanguages(anyLong(), eq(mContext), eq("en"), eq("de"), eq("de,en"));
        mContext.setTranslationLanguages("en", "de", "de,en");
        doNothing()
                .when(mContextJniMock)
                .setTranslationLanguages(anyLong(), eq(mContext), eq(""), eq(""), eq("de,en"));
        mContext.setTranslationLanguages("de", "de", "de,en");
    }
}
