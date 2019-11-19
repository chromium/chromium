// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;

/**
 * Tests parts of the ContextualSearchContext class.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ContextualSearchContextTest {
    private static final int INVALID = ContextualSearchContext.INVALID_OFFSET;
    private static final String UTF_8 = "UTF-8";
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

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private ContextualSearchContext.Natives mContextJniMock;

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
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, barackStartOffset, barackEndOffset);
    }

    private void setupTapInBarack() {
        int barackBeforeROffset = "Now Ba".length();
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, barackBeforeROffset, barackBeforeROffset);
    }

    private void simulateSelectWordAroundCaret(int startAdjust, int endAdjust) {
        mContext.onSelectionAdjusted(startAdjust, endAdjust);
    }

    private void simulateResolve(int startAdjust, int endAdjust) {
        mContext.onSelectionAdjusted(startAdjust, endAdjust);
    }

    private void setupResolvingTapInBarak() {
        setupTapInBarack();
        mContext.setResolveProperties(HOME_COUNTRY, true, 0, 0);
    }

    private void setupResolvingTapInObama() {
        int obamaBeforeMOffset = "Now Barack Oba".length();
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, obamaBeforeMOffset, obamaBeforeMOffset);
        mContext.setResolveProperties(HOME_COUNTRY, true, 0, 0);
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testEmptyContext() {
        assertFalse(mContext.hasAnalyzedTap());
        assertFalse(mContext.hasValidTappedText());
        assertFalse(mContext.hasValidSelection());
        assertFalse(mContext.canResolve());
        assertNull(mContext.getWordTapped());
        assertNull(mContext.getWordPreviousToTap());
        assertNull(mContext.getWordFollowingTap());
        assertEquals(INVALID, mContext.getWordTappedOffset());
        assertEquals(INVALID, mContext.getTapOffsetWithinTappedWord());
        assertEquals(INVALID, mContext.getWordFollowingTapOffset());
        assertNull(mContext.getSurroundingText());
        assertEquals(INVALID, mContext.getSelectionStartOffset());
        assertEquals(INVALID, mContext.getSelectionEndOffset());
        assertNull(mContext.getEncoding());
        assertNull(mContext.getInitialSelectedWord());
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
        assertNull(mContext.getInitialSelectedWord());
        assertFalse(mDidSelectionChange);

        simulateSelectWordAroundCaret(-"Ba".length(), "rack".length());
        assertEquals("Barack", mContext.getInitialSelectedWord());
        assertEquals("Barack".length(),
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
        assertEquals("Barack", mContext.getInitialSelectedWord());
        assertEquals("Barack Obama".length(),
                mContext.getSelectionEndOffset() - mContext.getSelectionStartOffset());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisOfWord() {
        setupResolvingTapInBarak();
        assertEquals("Barack", mContext.getWordTapped());
        assertEquals("Ba".length(), mContext.getTapOffsetWithinTappedWord());
        assertEquals("Now ".length(), mContext.getWordTappedOffset());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisOfWordsPreviousAndFollowing() {
        setupResolvingTapInObama();
        assertEquals("Barack", mContext.getWordPreviousToTap());
        assertEquals("is", mContext.getWordFollowingTap());
        assertEquals("Now ".length(), mContext.getWordPreviousToTapOffset());
        assertEquals("Now Barack Obama ".length(), mContext.getWordFollowingTapOffset());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisAtStartOfText() {
        int startOffset = 0;
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, startOffset, startOffset);
        assertNull(mContext.getWordPreviousToTap());
        // We can't recognize the first word because we need a space before it to do so.
        assertNull(mContext.getWordTapped());
        assertNull(mContext.getWordFollowingTap());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisAtSecondWordOfText() {
        int secondWordOffset = "Now ".length();
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, secondWordOffset, secondWordOffset);
        assertNull(mContext.getWordPreviousToTap());
        assertEquals("Barack", mContext.getWordTapped());
        assertEquals("Obama", mContext.getWordFollowingTap());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisAtEndOfText() {
        int endOffset = SAMPLE_TEXT.length();
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, endOffset, endOffset);
        assertNull(mContext.getWordPreviousToTap());
        assertNull(mContext.getWordTapped());
        assertNull(mContext.getWordFollowingTap());
    }

    @Test
    @Feature({"ContextualSearch", "Context"})
    public void testAnalysisAtWordBeforeEndOfText() {
        int wordBeforeEndOffset = SAMPLE_TEXT.length() - "s ambiguous.".length();
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, wordBeforeEndOffset, wordBeforeEndOffset);
        assertEquals("Clinton", mContext.getWordPreviousToTap());
        assertEquals("is", mContext.getWordTapped());
        assertEquals("ambiguous", mContext.getWordFollowingTap());
    }
}
