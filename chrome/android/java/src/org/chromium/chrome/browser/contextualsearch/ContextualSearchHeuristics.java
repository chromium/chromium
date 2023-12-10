// Copyright 2016 The Chromium Authors
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

    /** Manages a set of heuristics. */
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

    /** Logs the condition state for all the Tap suppression heuristics. */
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
}
