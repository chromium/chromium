// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;

/**
 * Implements signals for Taps on short and long words.
 * This signal could be used for suppression when the word is short, so we aggregate-log too.
 * We log CTR to UMA for Taps on both short and long words.
 */
class TapWordLengthSuppression extends ContextualSearchHeuristic {
    private static final int MAXIMUM_SHORT_WORD_LENGTH = 3;
    private static final int MINIMUM_LONG_WORD_LENGTH = 10;

    private final boolean mIsShortWordSuppressionEnabled;
    private final boolean mIsNotLongWordSuppressionEnabled;
    private final boolean mIsShortWordConditionSatisfied;
    private final boolean mIsLongWordConditionSatisfied;
    private final String mWordTapped;

    /**
     * Constructs a heuristic to categorize the Tap based on word length of the tapped word.
     * @param contextualSearchContext The current {@link ContextualSearchContext} so we can inspect
     *        the word tapped.
     */
    TapWordLengthSuppression(ContextualSearchContext contextualSearchContext) {
        mWordTapped = contextualSearchContext.getWordTapped();
        mIsShortWordSuppressionEnabled = ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_SHORT_WORD_SUPPRESSION_ENABLED);
        mIsNotLongWordSuppressionEnabled = ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_NOT_LONG_WORD_SUPPRESSION_ENABLED);
        mIsShortWordConditionSatisfied = isTapOnShortWord();
        mIsLongWordConditionSatisfied = isTapOnLongWord();
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return mIsShortWordSuppressionEnabled && mIsShortWordConditionSatisfied
                || mIsNotLongWordSuppressionEnabled && !mIsLongWordConditionSatisfied;
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (wasActivatedByTap) {
            ContextualSearchUma.logTapShortWordSeen(
                    wasSearchContentViewSeen, mIsShortWordConditionSatisfied);
            // Log CTR of long words, since not-long word CTR is probably not useful.
            ContextualSearchUma.logTapLongWordSeen(
                    wasSearchContentViewSeen, mIsLongWordConditionSatisfied);
        }
    }

    @Override
    protected boolean shouldAggregateLogForTapSuppression() {
        return true;
    }

    @Override
    protected boolean isConditionSatisfiedForAggregateLogging() {
        // Short-word suppression is a good candidate for aggregate logging of overall suppression.
        return mIsShortWordConditionSatisfied;
    }

    @Override
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder logger) {
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.IS_SHORT_WORD,
                mIsShortWordConditionSatisfied);
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.IS_LONG_WORD,
                mIsLongWordConditionSatisfied);
    }

    /**
     * @return Whether the tap is on a word whose length is considered short.
     */
    private boolean isTapOnShortWord() {
        // If setup failed, return false.
        return !TextUtils.isEmpty(mWordTapped) && mWordTapped.length() <= MAXIMUM_SHORT_WORD_LENGTH;
    }

    /**
     * @return Whether the tap is on a word whose length is considered long.
     */
    private boolean isTapOnLongWord() {
        // If setup failed, return false.
        return !TextUtils.isEmpty(mWordTapped) && mWordTapped.length() >= MINIMUM_LONG_WORD_LENGTH;
    }
}
