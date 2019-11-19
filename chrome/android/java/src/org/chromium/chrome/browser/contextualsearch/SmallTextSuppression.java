// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;

/**
 * Implements a policy that a Tap on a relatively small font should be ignored.
 */
class SmallTextSuppression extends ContextualSearchHeuristic {
    private static final int SMALL_SIZE_THRESHOLD_DIPS = 15;
    private static final int DEFAULT_DECILIZED_VALUE = 0;
    private static final float DECILIZE_SCALE_FACTOR = 0.5f;
    private static final int DECILIZE_MINIMUM_INPUT = 8;

    private final boolean mIsSuppressionEnabled;
    private final boolean mIsConditionSatisfied;
    private final int mDecilizedFontSize;

    /**
     * Constructs a heuristic to determine if the current Tap is on text with a small height.
     * @param fontSizeDips The size of the font from Blink in dips.
     */
    SmallTextSuppression(int fontSizeDips) {
        mIsSuppressionEnabled = ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_SMALL_TEXT_SUPPRESSION_ENABLED);
        mIsConditionSatisfied = isConditionSatisfied(fontSizeDips);
        mDecilizedFontSize = decilizedFontSize(fontSizeDips);
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
        logger.logFeature(
                ContextualSearchInteractionRecorder.Feature.FONT_SIZE, mDecilizedFontSize);
    }

    /**
     * Whether the conditions are satisfied to suppress the tap based on the given params:
     * @param fontSizeDips The size of the font in DIPs.
     * @return whether the conditions are satisfied to suppress (but might not actually do so).
     */
    private boolean isConditionSatisfied(int fontSizeDips) {
        return fontSizeDips != 0 && fontSizeDips < SMALL_SIZE_THRESHOLD_DIPS;
    }

    /**
     * Converts the input value into a "decile", an int in the range 0-10 inclusive.
     * @param value Any value to be scaled.  Very large values will pin at 10. Only an input of
     *        0 will return 0.
     * @return A value that's 0 if the input is zero and least 1 and at most 10 otherwise.
     */
    private int decilizedFontSize(int value) {
        if (value == 0) return DEFAULT_DECILIZED_VALUE;

        return clamp(Math.round(DECILIZE_SCALE_FACTOR * (value - DECILIZE_MINIMUM_INPUT)));
    }
}
