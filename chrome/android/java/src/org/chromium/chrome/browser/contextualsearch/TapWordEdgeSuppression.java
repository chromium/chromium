// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;

/**
 * Implements the policy that a Tap relatively far away from the middle of a word should be
 * ignored.  When a Tap is close to the middle of the word tapped it's treated normally.
 */
class TapWordEdgeSuppression extends ContextualSearchHeuristic {
    private static final int INVALID_OFFSET = ContextualSearchContext.INVALID_OFFSET;
    private static final int MIN_WORD_LENGTH = 4;
    private static final double MIN_WORD_START_RATIO = 0.25;
    private static final double MIN_WORD_END_RATIO = 0.25;

    private final boolean mIsSuppressionEnabled;
    private final boolean mIsConditionSatisfied;

    /**
     * Constructs a heuristic to determine if the current Tap is close to the edge of the word.
     * @param contextualSearchContext The current {@link ContextualSearchContext} so we can figure
     *        out what part of the word has been tapped.
     */
    TapWordEdgeSuppression(ContextualSearchContext contextualSearchContext) {
        mIsSuppressionEnabled = ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_WORD_EDGE_SUPPRESSION_ENABLED);
        mIsConditionSatisfied = isTapNearWordEdge(contextualSearchContext);
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return mIsSuppressionEnabled && mIsConditionSatisfied;
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (wasActivatedByTap) {
            ContextualSearchUma.logTapOnWordMiddleSeen(
                    wasSearchContentViewSeen, !mIsConditionSatisfied);
        }
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
        logger.logFeature(
                ContextualSearchInteractionRecorder.Feature.IS_WORD_EDGE, mIsConditionSatisfied);
    }

    /**
     * Whether the tap is near the word edge and not a second-tap (tap following a suppressed tap).
     * @param contextualSearchContext The {@link ContextualSearchContext}, used to determine how
     *        close the tap offset is to the word edges.
     * @return Whether the tap is near an edge and not a second tap.
     */
    private boolean isTapNearWordEdge(ContextualSearchContext contextualSearchContext) {
        // If setup failed, don't suppress.
        String wordTapped = contextualSearchContext.getWordTapped();
        int tapOffset = contextualSearchContext.getTapOffsetWithinTappedWord();
        if (TextUtils.isEmpty(wordTapped) || tapOffset == INVALID_OFFSET) return false;

        // If the word is long enough, suppress if the tap was near one end or the other.
        boolean isInStartEdge = (double) tapOffset / wordTapped.length() < MIN_WORD_START_RATIO;
        boolean isInEndEdge = (double) (wordTapped.length() - tapOffset) / wordTapped.length()
                < MIN_WORD_END_RATIO;
        if (wordTapped.length() >= MIN_WORD_LENGTH && (isInStartEdge || isInEndEdge)) return true;

        return false;
    }
}
