// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSetting;

/**
 * Provides a signal for the duration of a Tap being either brief or lengthy.
 * This signal could be used for suppression of taps below some threshold, so we aggregate-log too.
 * We log CTR to UMA for Taps shorter and longer than the threshold.
 */
class TapDurationSuppression extends ContextualSearchHeuristic {
    private static final int DEFAULT_TAP_DURATION_THRESHOLD_MS = 70;

    private final int mTapDurationMs;
    private final int mTapDurationThresholdMs;
    private final boolean mIsConditionSatisfied;

    /**
     * Constructs a heuristic to categorize the Tap based on duration as either short or long.
     * @param tapDurationMs The duration of the tap in milliseconds.
     */
    TapDurationSuppression(int tapDurationMs) {
        mTapDurationMs = tapDurationMs;
        mTapDurationThresholdMs = ContextualSearchFieldTrial.getValue(
                ContextualSearchSetting.TAP_DURATION_THRESHOLD_MS);
        int tapDurationThreshold = mTapDurationThresholdMs != 0 ? mTapDurationThresholdMs
                                                                : DEFAULT_TAP_DURATION_THRESHOLD_MS;
        mIsConditionSatisfied = tapDurationMs < tapDurationThreshold;
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return mIsConditionSatisfied && mTapDurationThresholdMs != 0;
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (wasActivatedByTap) {
            ContextualSearchUma.logTapDurationSeen(wasSearchContentViewSeen, mIsConditionSatisfied);

            // TODO(donnd): remove when these histograms have been analyzed.
            ContextualSearchUma.logTapDuration(wasSearchContentViewSeen, mTapDurationMs);
        }
    }

    @Override
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder logger) {
        logger.logFeature(
                ContextualSearchInteractionRecorder.Feature.TAP_DURATION_MS, mTapDurationMs);
    }

    @Override
    protected boolean shouldAggregateLogForTapSuppression() {
        return true;
    }

    @Override
    protected boolean isConditionSatisfiedForAggregateLogging() {
        return mIsConditionSatisfied;
    }
}
