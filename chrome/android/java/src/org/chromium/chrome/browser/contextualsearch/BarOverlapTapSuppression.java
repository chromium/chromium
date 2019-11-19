// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Heuristic for Tap suppression in areas where the Bar would overlap the selection.
 * Handles logging of results seen and activation.
 */
public class BarOverlapTapSuppression extends ContextualSearchHeuristic {
    private final ChromeActivity mActivity;
    private final boolean mIsConditionSatisfied;
    private final boolean mIsEnabled;
    private final float mPxToDp;

    /**
     * Constructs a Tap suppression heuristic that handles a Tap near where the Bar shows.
     * @param selectionController The {@link ContextualSearchSelectionController}.
     * @param y The y position of the Tap.
     */
    BarOverlapTapSuppression(
            ContextualSearchSelectionController selectionController, int y) {
        // TODO(donnd): rather than getting the Activity, find a way to access the panel
        // and ask it to determine overlap.  E.g. isCoordinateInsidePeekingBarArea(x, y) modeled
        // after isCoordinateInsideBar(x, y).
        mPxToDp = selectionController.getPxToDp();
        mActivity = selectionController.getActivity();
        mIsEnabled = ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_BAR_OVERLAP_SUPPRESSION_ENABLED);
        mIsConditionSatisfied = doesBarOverlap(y);
    }

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return mIsEnabled && mIsConditionSatisfied;
    }

    @Override
    protected void logConditionState() {
        if (mIsEnabled) {
            ContextualSearchUma.logBarOverlapSuppression(mIsConditionSatisfied);
        }
    }

    @Override
    protected void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        if (ContextualSearchFieldTrial.getSwitch(
                    ContextualSearchSwitch.IS_BAR_OVERLAP_COLLECTION_ENABLED)) {
            ContextualSearchUma.logBarOverlapResultsSeen(
                    wasSearchContentViewSeen, wasActivatedByTap, mIsConditionSatisfied);
        }
    }

    @Override
    protected void logPanelViewedDurations(long panelViewDurationMs, long panelOpenDurationMs) {
        if (panelOpenDurationMs > 0) {
            long panelPeekedDuration = panelViewDurationMs - panelOpenDurationMs;
            assert panelPeekedDuration >= 0;
            ContextualSearchUma.logBarOverlapPeekDuration(
                    mIsConditionSatisfied, panelPeekedDuration);
        }
    }

    @Override
    protected boolean shouldAggregateLogForTapSuppression() {
        return true;
    }

    @Override
    protected boolean isConditionSatisfiedForAggregateLogging() {
        return !mIsEnabled && mIsConditionSatisfied;
    }

    @Override
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder recorder) {
        recorder.logFeature(ContextualSearchInteractionRecorder.Feature.WAS_SCREEN_BOTTOM,
                mIsConditionSatisfied);
    }

    /**
     * @return The height of the content view area of the base page in pixels, or 0 if the
     *         Height cannot be reliably obtained.
     */
    private float getContentHeightPx() {
        Tab currentTab = mActivity.getActivityTab();
        ChromeFullscreenManager fullscreenManager = mActivity.getFullscreenManager();
        if (currentTab == null) return 0.f;

        float topControlsOffset = fullscreenManager.getTopControlOffset();
        float topControlsHeight = fullscreenManager.getTopControlsHeight();
        float bottomControlsOffset = fullscreenManager.getBottomControlOffset();
        float bottomControlsHeight = fullscreenManager.getBottomControlsHeight();

        float tabHeight = currentTab.getHeight();
        return (tabHeight - (topControlsHeight + topControlsOffset))
                - (bottomControlsHeight - bottomControlsOffset);
    }

    /**
     * @return Whether the Bar would overlap the given y coordinate when in its normal
     *         peeking state.
     */
    private boolean doesBarOverlap(int y) {
        float contentHeightPx = getContentHeightPx();
        if (contentHeightPx == 0) return false;

        // First check vertical overlap.
        // TODO(donnd): Ask the panel whether the bar overlaps!
        float barHeightDp = 56; // DPs
        float yDp = y * mPxToDp;
        float contentHeightDp = contentHeightPx * mPxToDp;

        return yDp >= (contentHeightDp - barHeightDp);
    }
}
