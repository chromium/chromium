// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;

/**
 * Defines the interface between a {@link ContextualSearchSelectionController} and the code that
 * handles callbacks in {@link ContextualSearchManager}.
 */
@NullMarked
interface ContextualSearchSelectionHandler {
    /** Handle a scroll event on the base page. */
    void handleScrollStart();

    /** Handle a scroll-ending event on the base page. */
    void handleScrollEnd();

    /** Handle the selection being cleared on the base page. */
    void handleSelectionCleared();

    /** Handle a valid tap gesture on the base page. */
    void handleValidTap();

    /** Handle an invalid tap gesture on the base page. */
    void handleInvalidTap();

    /** Handle a new selection of the given type, created at the given x,y position. */
    void handleSelection(
            String selection, boolean selectionValid, @SelectionType int type, float x, float y);

    /**
     * Handle a modification to the selection, done at the given x,y position.
     *
     * @param selection The new selection.
     * @param selectionValid Whether the new selection is valid.
     * @param x The x position of the adjustment.
     * @param y The y position of the adjustment.
     */
    void handleSelectionModification(String selection, boolean selectionValid, float x, float y);

    /** Handle a dismissal of the selection on the base page. */
    void handleSelectionDismissal();

    /** Handle suppression of a Tap gesture. */
    void handleSuppressedTap();

    /**
     * Handle a Tap gesture that has not been suppressed by showing the Tap Search UI.
     *
     * @param tapTimeNanoseconds When the last tap gesture happened, or {@code 0} if not known.
     */
    void handleNonSuppressedTap(long tapTimeNanoseconds);

    /**
     * Handle updating metrics to reflect that a Tap gesture <i>would</i> be suppressed for the
     * given heuristics.
     *
     * @param tapHeuristics The set of heuristics that would suppress the Tap.
     */
    void handleMetricsForWouldSuppressTap(ContextualSearchHeuristics tapHeuristics);

    /**
     * Handles a long-press gesture that may make a server Resolve request to determine the search.
     */
    void handleValidResolvingLongpress();
}
