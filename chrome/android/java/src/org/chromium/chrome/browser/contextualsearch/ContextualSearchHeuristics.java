// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import java.util.HashSet;
import java.util.Set;

/**
 * A set of {@link ContextualSearchHeuristic}s that support experimentation and logging for Tap
 * suppression.
 */
public class ContextualSearchHeuristics {
    protected Set<ContextualSearchHeuristic> mHeuristics;
    private QuickAnswersHeuristic mQuickAnswersHeuristic;

    /**
     * Manages a set of heuristics.
     */
    ContextualSearchHeuristics() {
        mHeuristics = new HashSet<ContextualSearchHeuristic>();
    }

    /**
     * Logs the results seen for the heuristics and whether they would have activated if enabled.
     * @param wasSearchContentViewSeen Whether the panel contents were seen.
     * @param wasActivatedByTap Whether the panel was activated by a Tap or not.
     */
    public void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            heuristic.logResultsSeen(wasSearchContentViewSeen, wasActivatedByTap);
        }
    }

    /**
     * Optionally logs data about the duration the panel was viewed and /or opened.
     * Default is to not log anything.
     * @param panelViewDurationMs The duration that the panel was viewed (Peek and opened) by the
     *        user.  This should always be a positive number, since this method is only called when
     *        the panel has been viewed (Peeked).
     * @param panelOpenDurationMs The duration that the panel was opened, or 0 if it was never
     *        opened.
     */
    public void logPanelViewedDurations(long panelViewDurationMs, long panelOpenDurationMs) {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            heuristic.logPanelViewedDurations(panelViewDurationMs, panelOpenDurationMs);
        }
    }

    /**
     * Logs the condition state for all the Tap suppression heuristics.
     */
    public void logContitionState() {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            heuristic.logConditionState();
        }
    }

    /**
     * Adds the given heuristic to the current set being managed.
     * @param heuristicToAdd Another heuristic to manage.
     */
    void add(ContextualSearchHeuristic heuristicToAdd) {
        mHeuristics.add(heuristicToAdd);
    }

    /**
     * @return Whether any heuristic that should be considered for aggregate tap suppression logging
     *         is satisfied regardless of whether the tap was actually suppressed.
     */
    public boolean isAnyConditionSatisfiedForAggregrateLogging() {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            if (heuristic.shouldAggregateLogForTapSuppression()
                    && heuristic.isConditionSatisfiedForAggregateLogging()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Logs all the heuristics that want to provide a Ranker "feature" to the given recorder.
     * @param recorder The recorder to log to.
     */
    public void logRankerTapSuppression(ContextualSearchInteractionRecorder recorder) {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            heuristic.logRankerTapSuppression(recorder);
        }
    }

    /**
     * Logs all the heuristics that want to provide outcomes to Ranker to the given recorder.
     * @param recorder The logger to log to.
     */
    public void logRankerTapSuppressionOutcome(ContextualSearchInteractionRecorder recorder) {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            heuristic.logRankerTapSuppressionOutcome(recorder);
        }
    }

    /**
     * Sets the {@link QuickAnswersHeuristic} so that it can be accessed externally by
     * {@link #getQuickAnswersHeuristic}.
     * @param quickAnswersHeuristic The active {@link QuickAnswersHeuristic}.
     */
    public void setQuickAnswersHeuristic(QuickAnswersHeuristic quickAnswersHeuristic) {
        mQuickAnswersHeuristic = quickAnswersHeuristic;
    }

    /**
     * @return The active {@link QuickAnswersHeuristic}.
     */
    public QuickAnswersHeuristic getQuickAnswersHeuristic() {
        return mQuickAnswersHeuristic;
    }
}
