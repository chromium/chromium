// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;

/**
 * A set of {@link ContextualSearchHeuristic}s that support experimentation and logging
 * and can be used as signals to drive an ML tap suppression model.
 */
public class TapSuppressionHeuristics extends ContextualSearchHeuristics {
    /**
     * Gets all the heuristics needed for Tap suppression.
     * @param selectionController The {@link ContextualSearchSelectionController}.
     * @param previousTapState The state of the previous tap, or {@code null}.
     * @param x The x position of the Tap.
     * @param y The y position of the Tap.
     * @param wasSelectionEmptyBeforeTap Whether the selection was empty before this tap.
     */
    TapSuppressionHeuristics(
            ContextualSearchSelectionController selectionController,
            @Nullable ContextualSearchTapState previousTapState,
            int x,
            int y,
            boolean wasSelectionEmptyBeforeTap) {
        super();
        mHeuristics.add(
                new TapFarFromPreviousSuppression(
                        selectionController, previousTapState, x, y, wasSelectionEmptyBeforeTap));
    }

    @Override
    public void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            heuristic.logResultsSeen(wasSearchContentViewSeen, wasActivatedByTap);
        }
    }

    /** Logs the condition state for all the Tap suppression heuristics. */
    void logConditionState() {
        // TODO(donnd): consider doing this logging automatically in the constructor rather than
        // leaving this an optional separate method.
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            heuristic.logConditionState();
        }
    }

    /**
     * @return Whether the Tap should be suppressed (due to a heuristic condition being satisfied).
     */
    boolean shouldSuppressTap() {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            if (heuristic.isConditionSatisfiedAndEnabled()) return true;
        }
        return false;
    }
}
