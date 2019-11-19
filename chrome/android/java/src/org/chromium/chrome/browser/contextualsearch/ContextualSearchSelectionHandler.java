// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;

/**
 * Defines the interface between a {@link ContextualSearchSelectionController} and the code that
 * handles callbacks.
 */
interface ContextualSearchSelectionHandler {
    /**
     * Handle a scroll event on the base page.
     */
    public void handleScrollStart();

    /**
     * Handle a scroll-ending event on the base page.
     */
    public void handleScrollEnd();

    /**
     * Handle the selection being cleared on the base page.
     */
    public void handleSelectionCleared();

    /**
     * Handle a valid tap gesture on the base page.
     */
    public void handleValidTap();

    /**
     * Handle an invalid tap gesture on the base page.
     */
    public void handleInvalidTap();

    /**
     * Handle a new selection of the given type, created at the given x,y position.
     */
    public void handleSelection(
            String selection, boolean selectionValid, @SelectionType int type, float x, float y);

    /**
     * Handle a modification to the selection, done at the given x,y position.
     * @param selection The new selection.
     * @param selectionValid Whether the new selection is valid.
     * @param x The x position of the adjustment.
     * @param y The y position of the adjustment.
     */
    public void handleSelectionModification(
            String selection, boolean selectionValid, float x, float y);

    /**
     * Handle a dismissal of the selection on the base page.
     */
    public void handleSelectionDismissal();

    /**
     * Handle suppression of a Tap gesture.
     */
    public void handleSuppressedTap();

    /**
     * Handle a Tap gesture that has not been suppressed by showing the Tap Search UI.
     * @param tapTimeNanoseconds When the last tap gesture happened, or {@code 0} if not known.
     */
    public void handleNonSuppressedTap(long tapTimeNanoseconds);

    /**
     * Handle updating metrics to reflect that a Tap gesture <i>would</i> be suppressed
     * for the given heuristics.
     * @param tapHeuristics The set of heuristics that would suppress the Tap.
     */
    public void handleMetricsForWouldSuppressTap(ContextualSearchHeuristics tapHeuristics);

    /**
     * Logs all the features that we can obtain without accessing heuristics, i.e. from global
     * state.
     * @param interactionRecorder The {@link ContextualSearchInteractionRecorder} to log the
     * features to.
     */
    public void logNonHeuristicFeatures(ContextualSearchInteractionRecorder interactionRecorder);

    /**
     * Handles a long-press gesture that may make a server Resolve request to determine the search.
     */
    void handleValidResolvingLongpress();
}
