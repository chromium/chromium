// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;

/**
 * A set of {@link ContextualSearchHeuristic}s that support experimentation and logging.
 */
public class TapSuppressionHeuristics extends ContextualSearchHeuristics {
    private CtrSuppression mCtrSuppression;

    /**
     * Gets all the heuristics needed for Tap suppression.
     * @param selectionController The {@link ContextualSearchSelectionController}.
     * @param previousTapState The state of the previous tap, or {@code null}.
     * @param x The x position of the Tap.
     * @param y The y position of the Tap.
     * @param contextualSearchContext The {@link ContextualSearchContext} of this tap.
     * @param tapDurationMs The duration of this tap in milliseconds.
     * @param wasSelectionEmptyBeforeTap Whether the selection was empty before this tap.
     * @param fontSizeDips The font size from Blink in dips.
     * @param elementRunLength The length of the text in the element tapped, in characters.
     */
    TapSuppressionHeuristics(ContextualSearchSelectionController selectionController,
            @Nullable ContextualSearchTapState previousTapState, int x, int y,
            ContextualSearchContext contextualSearchContext, int tapDurationMs,
            boolean wasSelectionEmptyBeforeTap, int fontSizeDips, int elementRunLength) {
        super();
        mCtrSuppression = new CtrSuppression();
        mHeuristics.add(mCtrSuppression);
        mHeuristics.add(new EngagementSuppression());
        mHeuristics.add(new RecentScrollTapSuppression(selectionController));
        mHeuristics.add(new TapFarFromPreviousSuppression(
                selectionController, previousTapState, x, y, wasSelectionEmptyBeforeTap));
        mHeuristics.add(new TapDurationSuppression(tapDurationMs));
        mHeuristics.add(new TapWordLengthSuppression(contextualSearchContext));
        mHeuristics.add(new TapWordEdgeSuppression(contextualSearchContext));
        mHeuristics.add(new ContextualSearchEntityHeuristic(contextualSearchContext));
        mHeuristics.add(new NearTopTapSuppression(selectionController, y));
        mHeuristics.add(new BarOverlapTapSuppression(selectionController, y));
        mHeuristics.add(new ShortTextRunSuppression(contextualSearchContext, elementRunLength));
        mHeuristics.add(new SmallTextSuppression(fontSizeDips));
        // Second Tap ML Override.
        mHeuristics.add(new SecondTapMlOverride(selectionController, previousTapState, x, y));
        // Quick Answer that appears in the Caption via the JS API.
        QuickAnswersHeuristic quickAnswersHeuristic = new QuickAnswersHeuristic();
        setQuickAnswersHeuristic(quickAnswersHeuristic);
        mHeuristics.add(quickAnswersHeuristic);
    }

    /**
     * This method should be called to clean up storage when an instance of this class is
     * no longer in use.
     */
    public void destroy() {
        if (mCtrSuppression != null) {
            mCtrSuppression.destroy();
            mCtrSuppression = null;
        }
    }

    @Override
    public void logResultsSeen(boolean wasSearchContentViewSeen, boolean wasActivatedByTap) {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            heuristic.logResultsSeen(wasSearchContentViewSeen, wasActivatedByTap);
        }
    }

    /**
     * Logs the condition state for all the Tap suppression heuristics.
     */
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

    /**
     * @return Whether the Tap should override an ML suppression.
     */
    boolean shouldOverrideMlTapSuppression() {
        for (ContextualSearchHeuristic heuristic : mHeuristics) {
            if (heuristic.shouldOverrideMlTapSuppression()) return true;
        }
        return false;
    }
}
