// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;

/**
 * Implements a policy that a Tap on a word that's part of a short text run should be suppressed.
 * Computes the ratio of the tapped word to the entire run of text in the element (which includes
 * style changes).  This means that short navigational elements will have a high score, and a tap
 * in a longer paragraph will have a smaller score.
 */
class ShortTextRunSuppression extends ContextualSearchHeuristic {
    // Threshold for a "large" ratio of word to element.
    private static final int LARGE_WORD_ELEMENT_RATIO = 3;

    // Value to use when the word or element length is unavailable.
    private static final int DEFAULT_WORD_ELEMENT_RATIO = 0;

    private final boolean mIsSuppressionEnabled;
    private final boolean mIsConditionSatisfied;
    private final int mWordElementRatioDecile;

    /**
     * Constructs a heuristic to determine if the current Tap is in a short text run.
     * @param contextualSearchContext The current {@link ContextualSearchContext}, so we can get
     *        the length of the word tapped.
     * @param elementRunLength The length of the text in the element tapped, in characters.
     */
    ShortTextRunSuppression(ContextualSearchContext contextualSearchContext, int elementRunLength) {
        mIsSuppressionEnabled = ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_SHORT_TEXT_RUN_SUPPRESSION_ENABLED);
        mWordElementRatioDecile = wordElementRatio(contextualSearchContext, elementRunLength);
        mIsConditionSatisfied = mWordElementRatioDecile >= LARGE_WORD_ELEMENT_RATIO;
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return mIsSuppressionEnabled && mIsConditionSatisfied;
    }

    @Override
    protected boolean shouldAggregateLogForTapSuppression() {
        return true;
    }

    @Override
    protected boolean isConditionSatisfiedForAggregateLogging() {
        return mIsConditionSatisfied;
    }

    @Override
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder logger) {
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.PORTION_OF_ELEMENT,
                mWordElementRatioDecile);
    }

    /**
     * Returns the ratio of word-length to the element-length scaled to a range from 1 to 10.
     * @param contextualSearchContext The {@link ContextualSearchContext} that knows about the word
     *        tapped.
     * @param elementRunLength The length of the element containing the tapped word, in characters.
     * @return A value in the range 0-10 with 0 meaning no ratio could be computed and larger values
     *         reflecting higher word/element ratios.
     */
    private int wordElementRatio(
            ContextualSearchContext contextualSearchContext, int elementRunLength) {
        // If setup failed, don't suppress.
        String wordTapped = contextualSearchContext.getWordTapped();
        if (wordTapped == null || elementRunLength == 0) return DEFAULT_WORD_ELEMENT_RATIO;

        return clamp((int) ((float) wordTapped.length() / elementRunLength * 10));
    }
}
