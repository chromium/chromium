// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;

/**
 * An interface for driving operations in the Contextual Search Manager's internal state by the
 * {@link ContextualSearchInternalStateController} class.
 */
public interface ContextualSearchInternalStateHandler {
    /**
     * Hides the Contextual Search user interface.
     * @see ContextualSearchInternalStateController.InternalState#IDLE
     */
    void hideContextualSearchUi(@StateChangeReason int reason);

    /**
     * Shows the Contextual Search user interface for a resolving search.
     * @see ContextualSearchInternalStateController.InternalState#SHOW_RESOLVING_UI
     */
    void showContextualSearchResolvingUi();

    /**
     * Shows the Contextual Search user interface for a Long-press.
     * @see ContextualSearchInternalStateController.InternalState#SHOWING_LONGPRESS_SEARCH
     */
    void showContextualSearchLongpressUi();

    /**
     * The first state in the Tap-gesture processing pipeline where we know we're processing
     * a Tap-gesture that won't be converted into something else.  Starts the processing pipeline.
     * @see ContextualSearchInternalStateController.InternalState#TAP_GESTURE_COMMIT
     */
    void tapGestureCommit();

    /**
     * Gathers text surrounding the current selection, which may have been created by either a Tap
     * or a Long-press gesture.
     * @see ContextualSearchInternalStateController.InternalState#GATHERING_SURROUNDINGS
     */
    void gatherSurroundingText();

    /**
     * Starts the process of deciding if we'll suppress the current gesture or not.
     * @see ContextualSearchInternalStateController.InternalState#DECIDING_SUPPRESSION
     */
    void decideSuppression();

    /**
     * Starts the process of selecting a word around the current caret.
     * @see ContextualSearchInternalStateController.InternalState#START_SHOWING_TAP_UI
     */
    void startShowingTapUi();

    /**
     * Waits to see if a Tap gesture will be made when the selection has been cleared, which allows
     * a Tap near a previous Tap to be handled without a hide/show of the UI.
     * @see
     * ContextualSearchInternalStateController.InternalState#WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS
     */
    void waitForPossibleTapNearPrevious();

    /**
     * Waits to see if a Tap gesture will be made on a previous Tap-selection when a Tap was
     * recognized.
     * @see ContextualSearchInternalStateController.InternalState#
     * WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION
     */
    void waitForPossibleTapOnTapSelection();

    /**
     * Starts a Resolve request to our server for the best Search Term.
     * @see ContextualSearchInternalStateController.InternalState#RESOLVING
     */
    void resolveSearchTerm();
}
