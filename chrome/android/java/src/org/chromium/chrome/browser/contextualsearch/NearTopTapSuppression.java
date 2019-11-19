// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSetting;

/**
 * Heuristic for Tap suppression near the top of the content view area.
 * Handles logging of results seen and the condition state.
 */
public class NearTopTapSuppression extends ContextualSearchHeuristic {
    private final int mExperiementThresholdDps;
    private final boolean mIsConditionSatisfied;
    private final int mYDp;

    /**
     * Constructs a Tap suppression heuristic that handles a Tap after a recent scroll.
     * This logs activation data that includes whether it activated for a threshold specified
     * by an experiment. This also logs Results-seen data to profile when results are seen relative
     * to a recent scroll.
     * @param selectionController The {@link ContextualSearchSelectionController}.
     */
    NearTopTapSuppression(ContextualSearchSelectionController selectionController, int y) {
        mExperiementThresholdDps = ContextualSearchFieldTrial.getValue(
                ContextualSearchSetting.SCREEN_TOP_SUPPRESSION_DPS);
        mYDp = (int) (y * selectionController.getPxToDp());
        mIsConditionSatisfied = mYDp < mExperiementThresholdDps;
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return mIsConditionSatisfied;
    }

    @Override
    protected void logConditionState() {
        if (mExperiementThresholdDps > 0) {
            ContextualSearchUma.logScreenTopTapSuppression(mIsConditionSatisfied);
        }
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (wasActivatedByTap) {
            ContextualSearchUma.logScreenTopTapLocation(
                    wasSearchContentViewSeen, wasActivatedByTap, mYDp);
        }
    }

    @Override
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder logger) {
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.SCREEN_TOP_DPS, mYDp);
    }

    // TODO(twellington): Define a default value to use when determining if the condition is
    // satisfied for logging.
}
