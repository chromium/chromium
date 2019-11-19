// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;

/**
 * Heuristic that allows a second tap near a previous ML-suppressed tap to override suppression.
 */
class SecondTapMlOverride extends ContextualSearchHeuristic {
    // Tap must be between about 1/4 second and 3 seconds after the previous tap.
    private static final long TIME_THRESHOLD_MAX_NANOSECONDS = 3_000_000_000L;
    private static final long TIME_THRESHOLD_MIN_NANOSECONDS = 200_000_000L;
    // Tap must be within 30 dips of the previous tap.
    private static final int TAP_RADIUS_DPS = 30;

    private final boolean mIsSecondTapEnabled;
    private final float mPxToDp;
    private final boolean mIsConditionSatisfied; // whether to override suppression or not.

    /**
     * Constructs a heuristic to decide if a Second Tap should override ML suppression or not.
     * @param controller The Selection Controller.
     * @param previousTapState The specifics regarding the previous Tap.
     * @param x The x coordinate of the current tap.
     * @param y The y coordinate of the current tap.
     */
    SecondTapMlOverride(ContextualSearchSelectionController controller,
            ContextualSearchTapState previousTapState, int x, int y) {
        mIsSecondTapEnabled = ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_CONTEXTUAL_SEARCH_SECOND_TAP_ML_OVERRIDE_ENABLED);
        mPxToDp = controller.getPxToDp();

        mIsConditionSatisfied = previousTapState != null && previousTapState.wasMlSuppressed()
                && shouldOverrideSecondTap(previousTapState, x, y);
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return false;
    }

    @Override
    protected boolean shouldOverrideMlTapSuppression() {
        return mIsSecondTapEnabled && mIsConditionSatisfied;
    }

    @Override
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder logger) {
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.IS_SECOND_TAP_OVERRIDE,
                shouldOverrideMlTapSuppression());
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (wasActivatedByTap && shouldOverrideMlTapSuppression()) {
            ContextualSearchUma.logSecondTapMlOverrideResultsSeen(wasSearchContentViewSeen);
        }
    }

    /**
     * Determines whether a second tap at the given coordinates should override ML suppression.
     * @param tapState The specifics regarding the previous Tap.
     * @param x The x coordinate of the current tap.
     * @param y The y coordinate of the current tap.
     * @return whether a second tap at the given coordinates should override or not.
     */
    private boolean shouldOverrideSecondTap(ContextualSearchTapState tapState, int x, int y) {
        // The second tap needs to be close to the first tap in both time and space.
        // Recent enough?
        long timeSinceLastTap = System.nanoTime() - tapState.tapTimeNanoseconds();
        if (timeSinceLastTap < TIME_THRESHOLD_MIN_NANOSECONDS
                || timeSinceLastTap > TIME_THRESHOLD_MAX_NANOSECONDS) {
            return false;
        }

        // Within our radius?
        float deltaXDp = (tapState.getX() - x) * mPxToDp;
        float deltaYDp = (tapState.getY() - y) * mPxToDp;
        // Use x^2 * y^2 = r^2
        float distanceSquaredDp = deltaXDp * deltaXDp + deltaYDp * deltaYDp;
        return distanceSquaredDp <= TAP_RADIUS_DPS * TAP_RADIUS_DPS;
    }
}
