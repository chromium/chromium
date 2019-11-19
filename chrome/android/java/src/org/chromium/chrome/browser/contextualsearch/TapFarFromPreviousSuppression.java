// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;

/**
 * Implements the policy that a Tap relatively far away from an existing Contextual Search
 * selection should just dismiss our UX.  When a Tap is close by, we assume the user must have
 * missed the original intended target so we reselect based on the new Tap location.
 */
class TapFarFromPreviousSuppression extends ContextualSearchHeuristic {
    private static final double RETAP_DISTANCE_SQUARED_DP = Math.pow(75, 2);

    private final ContextualSearchTapState mPreviousTapState;
    private final float mPxToDp;
    private final boolean mShouldHandleTap;

    /**
     * Constructs a heuristic to determine if the current Tap should be suppressed because it is
     * far from the previous tap.
     * @param controller The {@link ContextualSearchSelectionController}.
     * @param previousTapState The state of the previous tap gesture, or {@code null}.
     * @param x The x coordinate of the tap gesture.
     * @param y The y coordinate of the tap gesture.
     * @param wasSelectionEmptyBeforeTap Whether the selection was empty just before this tap.
     */
    TapFarFromPreviousSuppression(ContextualSearchSelectionController controller,
            @Nullable ContextualSearchTapState previousTapState, int x, int y,
            boolean wasSelectionEmptyBeforeTap) {
        mPxToDp = controller.getPxToDp();
        mPreviousTapState = previousTapState;
        mShouldHandleTap = shouldHandleTap(x, y, wasSelectionEmptyBeforeTap);
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return !mShouldHandleTap;
    }

    /**
     * Determines whether the tap should be handled based on whether it's near a previous tap and
     * whether the selection was visible just before that tap.  Uses the previous tap state.
     * @param x The x coordinate of the current tap.
     * @param y The y coordinate of the current tap.
     * @param wasSelectionEmptyBeforeTap Whether the selection was empty before the current tap.
     * @return whether a tap at the given coordinates should be handled or not.
     */
    private boolean shouldHandleTap(int x, int y, boolean wasSelectionEmptyBeforeTap) {
        if (mPreviousTapState == null || wasSelectionEmptyBeforeTap) return true;

        return wasTapCloseToPreviousTap(x, y);
    }

    /**
     * @return Whether a tap at the given coordinates is considered "close" to the previous tap.
     */
    private boolean wasTapCloseToPreviousTap(int x, int y) {
        float deltaXDp = (mPreviousTapState.getX() - x) * mPxToDp;
        float deltaYDp = (mPreviousTapState.getY() - y) * mPxToDp;
        float distanceSquaredDp = deltaXDp * deltaXDp + deltaYDp * deltaYDp;
        return distanceSquaredDp <= RETAP_DISTANCE_SQUARED_DP;
    }
}
