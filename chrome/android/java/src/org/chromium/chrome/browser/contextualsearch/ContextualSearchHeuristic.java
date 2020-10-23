// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

/**
 * A piece of conditional behavior that supports experimentation and logging.
 * This is used for Tap suppression and each heuristic feeds a datum into
 * the ML algorithm for suppression.
 */
abstract class ContextualSearchHeuristic {
    /**
     * Gets whether this heuristic's condition was satisfied or not if it is enabled.
     * In the case of a Tap heuristic, if the condition is satisfied the Tap is suppressed.
     * This heuristic may be called in logResultsSeen regardless of whether the condition was
     * satisfied.
     * @return True if this heuristic is enabled and its condition is satisfied, otherwise false.
     */
    protected abstract boolean isConditionSatisfiedAndEnabled();

    /**
     * Optionally logs this heuristic's condition state.  Up to the heuristic to determine exactly
     * what to log and whether to log at all.  Default is to not log anything.
     */
    protected void logConditionState() {}

    /**
     * Optionally logs whether results would have been seen if this heuristic had its condition
     * satisfied, and possibly some associated data for profiling (up to the heuristic to decide).
     * Default is to not log anything.
     * @param wasSearchContentViewSeen Whether the panel contents were seen.
     * @param wasActivatedByTap Whether the panel was activated by a Tap or not.
     */
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {}

    /**
     * Optionally logs data about the duration the panel was viewed and /or opened.
     * Default is to not log anything.
     * @param panelViewDurationMs The duration that the panel was viewed (Peek and opened) by the
     *        user.  This should always be a positive number, since this method is only called when
     *        the panel has been viewed (Peeked).
     * @param panelOpenDurationMs The duration that the panel was opened, or 0 if it was never
     *        opened.
     */
    protected void logPanelViewedDurations(long panelViewDurationMs, long panelOpenDurationMs) {}

    /**
     * @return Whether this heuristic should be considered when logging aggregate metrics for Tap
     *         suppression.
     */
    protected boolean shouldAggregateLogForTapSuppression() {
        return false;
    }

    /**
     * @return Whether this heuristic's condition would have been satisfied, causing a tap
     *         suppression, if it were enabled through VariationsAssociatedData. If the feature is
     *         enabled through VariationsAssociatedData then this method should return false.
     */
    protected boolean isConditionSatisfiedForAggregateLogging() {
        return false;
    }

    /**
     * Logs the heuristic to UMA and UKM through Ranker logging for the purpose of Tap Suppression.
     * @param recorder A logger to log to.
     */
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder recorder) {
        // Default is to not log.
    }

    /**
     * Logs a Ranker outcome using the heuristic for the purpose of Ranker Tap Suppression.
     * @param recorder A logger to log to.
     */
    protected void logRankerTapSuppressionOutcome(ContextualSearchInteractionRecorder recorder) {
        // Default is to not log.
    }

    /**
     * Allows a heuristic to override a machine-learning model's Tap Suppression.
     */
    protected boolean shouldOverrideMlTapSuppression() {
        return false;
    }

    /**
     * Clamps an input value into a range of 1-10 inclusive.
     * @param value The value to limit.
     * @return A value that's at least 1 and at most 10.
     */
    protected int clamp(int value) {
        return Math.max(1, Math.min(10, value));
    }
}
