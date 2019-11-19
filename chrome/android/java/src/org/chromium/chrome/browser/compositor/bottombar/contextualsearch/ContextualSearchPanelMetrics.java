// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchHeuristics;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchIPH;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInteractionRecorder;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.contextualsearch.EngagementSuppression;
import org.chromium.chrome.browser.contextualsearch.QuickActionCategory;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * This class is responsible for all the logging related to Contextual Search.
 */
public class ContextualSearchPanelMetrics {
    // Flags for logging.
    private boolean mDidSearchInvolvePromo;
    private boolean mWasSearchContentViewSeen;
    private boolean mIsPromoActive;
    private boolean mHasExpanded;
    private boolean mHasMaximized;
    private boolean mHasExitedPeeking;
    private boolean mHasExitedExpanded;
    private boolean mHasExitedMaximized;
    private boolean mIsSerpNavigation;
    private boolean mWasActivatedByTap;
    private boolean mWasPanelOpenedBeyondPeek;
    private boolean mWasContextualCardsDataShown;
    @ResolvedSearchTerm.CardTag
    private int mCardTag;
    private boolean mWasQuickActionShown;
    private int mQuickActionCategory;
    private boolean mWasQuickActionClicked;
    private int mSelectionLength;
    // Whether any Tap suppression heuristic was satisfied when the panel was shown.
    private boolean mWasAnyHeuristicSatisfiedOnPanelShow;
    // Time when the panel was triggered (not reset by a chained search).
    // Panel transitions are animated so mPanelTriggerTimeNs will be less than mFirstPeekTimeNs.
    private long mPanelTriggerTimeFromTapNs;
    // Time when the panel peeks into view (not reset by a chained search).
    // Used to log total time the panel is showing (not closed).
    private long mFirstPeekTimeNs;
    // Time when a search request was started. Reset by chained searches.
    // Used to log the time it takes for a Search Result to become available.
    private long mSearchRequestStartTimeNs;
    // Time when the panel was opened beyond peeked. Reset when the panel is closed.
    // Used to log how long the panel was open.
    private long mPanelOpenedBeyondPeekTimeNs;
    // Duration that the panel was opened beyond peeked. Reset when the panel is closed.
    // Used to log how long the panel was open.
    private long mPanelOpenedBeyondPeekDurationMs;
    // The current set of heuristics that should be logged with results seen when the panel closes.
    private ContextualSearchHeuristics mResultsSeenExperiments;
    // The interaction recorder to use to record results seen when the panel closes.
    private ContextualSearchInteractionRecorder mInteractionRecorder;
    // Whether interaction Outcomes are valid, because we showed the panel.
    private boolean mAreOutcomesValid;

    /**
     * Log information when the panel's state has changed.
     * @param fromState The state the panel is transitioning from.
     * @param toState The state that the panel is transitioning to.
     * @param reason The reason for the state change.
     * @param profile The current {@link Profile}.
     */
    public void onPanelStateChanged(@PanelState int fromState, @PanelState int toState,
            @StateChangeReason int reason, Profile profile) {
        // Note: the logging within this function includes the promo, unless specifically
        // excluded.
        boolean isStartingSearch = isStartingNewContextualSearch(toState, reason);
        boolean isEndingSearch = isEndingContextualSearch(fromState, toState, isStartingSearch);
        boolean isChained = isStartingSearch && isOngoingContextualSearch(fromState);
        boolean isSameState = fromState == toState;
        boolean isFirstExitFromPeeking = fromState == PanelState.PEEKED && !mHasExitedPeeking
                && (!isSameState || isStartingSearch);
        boolean isFirstExitFromExpanded = fromState == PanelState.EXPANDED && !mHasExitedExpanded
                && !isSameState;
        boolean isFirstExitFromMaximized = fromState == PanelState.MAXIMIZED && !mHasExitedMaximized
                && !isSameState;
        boolean isContentVisible =
                toState == PanelState.MAXIMIZED || toState == PanelState.EXPANDED;
        boolean isExitingPanelOpenedBeyondPeeked = mWasPanelOpenedBeyondPeek && !isContentVisible;

        if (toState == PanelState.CLOSED && mPanelTriggerTimeFromTapNs != 0
                && reason == StateChangeReason.BASE_PAGE_SCROLL) {
            long durationMs = (System.nanoTime() - mPanelTriggerTimeFromTapNs)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            ContextualSearchUma.logDurationBetweenTriggerAndScroll(
                    durationMs, mWasSearchContentViewSeen);
        }

        if (isExitingPanelOpenedBeyondPeeked) {
            assert mPanelOpenedBeyondPeekTimeNs != 0;
            mPanelOpenedBeyondPeekDurationMs = (System.nanoTime() - mPanelOpenedBeyondPeekTimeNs)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            ContextualSearchUma.logPanelOpenDuration(mPanelOpenedBeyondPeekDurationMs);
            mPanelOpenedBeyondPeekTimeNs = 0;
            mWasPanelOpenedBeyondPeek = false;
        }

        if (isEndingSearch) {
            long panelViewDurationMs =
                    (System.nanoTime() - mFirstPeekTimeNs) / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            ContextualSearchUma.logPanelViewDurationAction(panelViewDurationMs);
            if (!mDidSearchInvolvePromo) {
                // Measure duration only when the promo is not involved.
                ContextualSearchUma.logDuration(
                        mWasSearchContentViewSeen, isChained, panelViewDurationMs);
            }
            if (mIsPromoActive) {
                // The user is exiting still in the promo, without choosing an option.
                ContextualSearchUma.logPromoSeen(mWasSearchContentViewSeen, mWasActivatedByTap);
            } else {
                ContextualSearchUma.logResultsSeen(mWasSearchContentViewSeen, mWasActivatedByTap);
            }

            if (mWasContextualCardsDataShown) {
                ContextualSearchUma.logContextualCardsResultsSeen(mWasSearchContentViewSeen);
                EngagementSuppression.registerContextualCardsImpression(mWasSearchContentViewSeen);
            }
            ContextualSearchUma.logCardTagSeen(mWasSearchContentViewSeen, mCardTag);
            if (mWasQuickActionShown) {
                ContextualSearchUma.logQuickActionResultsSeen(mWasSearchContentViewSeen,
                        mQuickActionCategory);
                ContextualSearchUma.logQuickActionClicked(mWasQuickActionClicked,
                        mQuickActionCategory);
                EngagementSuppression.registerQuickActionImpression(
                        mWasSearchContentViewSeen, mWasQuickActionClicked);
            }

            if (mResultsSeenExperiments != null) {
                mResultsSeenExperiments.logResultsSeen(
                        mWasSearchContentViewSeen, mWasActivatedByTap);
                mResultsSeenExperiments.logPanelViewedDurations(
                        panelViewDurationMs, mPanelOpenedBeyondPeekDurationMs);
                if (!isChained) mResultsSeenExperiments = null;
            }
            mPanelOpenedBeyondPeekDurationMs = 0;

            if (mWasActivatedByTap) {
                boolean wasAnySuppressionHeuristicSatisfied = mWasAnyHeuristicSatisfiedOnPanelShow;
                ContextualSearchUma.logAnyTapSuppressionHeuristicSatisfied(
                        mWasSearchContentViewSeen, wasAnySuppressionHeuristicSatisfied);
                ContextualSearchUma.logSelectionLengthResultsSeen(
                        mWasSearchContentViewSeen, mSelectionLength);
                ContextualSearchUma.logTapResultsSeen(mWasSearchContentViewSeen);
            }
            ContextualSearchUma.logAllResultsSeen(mWasSearchContentViewSeen);

            // Notifications to Feature Engagement.
            ContextualSearchIPH.doSearchFinishedNotifications(profile, mWasSearchContentViewSeen,
                    mWasActivatedByTap, mWasContextualCardsDataShown);

            writeInteractionOutcomesAndReset();
        }

        if (isStartingSearch) {
            mFirstPeekTimeNs = System.nanoTime();
            mWasActivatedByTap = reason == StateChangeReason.TEXT_SELECT_TAP;
            if (mWasActivatedByTap && mResultsSeenExperiments != null) {
                mWasAnyHeuristicSatisfiedOnPanelShow =
                        mResultsSeenExperiments.isAnyConditionSatisfiedForAggregrateLogging();
            } else {
                mWasAnyHeuristicSatisfiedOnPanelShow = false;
            }
            mAreOutcomesValid = true;
        }

        // Log state changes. We only log the first transition to a state within a contextual
        // search. Note that when a user clicks on a link on the search content view, this will
        // trigger a transition to MAXIMIZED (SERP_NAVIGATION) followed by a transition to
        // CLOSED (TAB_PROMOTION). For the purpose of logging, the reason for the second transition
        // is reinterpreted to SERP_NAVIGATION, in order to distinguish it from a tab promotion
        // caused when tapping on the Search Bar when the Panel is maximized.
        @StateChangeReason
        int reasonForLogging = mIsSerpNavigation ? StateChangeReason.SERP_NAVIGATION : reason;
        if (isStartingSearch || isEndingSearch
                || (!mHasExpanded && toState == PanelState.EXPANDED)
                || (!mHasMaximized && toState == PanelState.MAXIMIZED)) {
            ContextualSearchUma.logFirstStateEntry(fromState, toState, reasonForLogging);
        }
        // Note: CLOSED / UNDEFINED state exits are detected when a search that is not chained is
        // starting.
        if ((isStartingSearch && !isChained) || isFirstExitFromPeeking || isFirstExitFromExpanded
                || isFirstExitFromMaximized) {
            ContextualSearchUma.logFirstStateExit(fromState, toState, reasonForLogging);
        }
        // Log individual user actions so they can be sequenced.
        ContextualSearchUma.logPanelStateUserAction(toState, reasonForLogging);

        // We can now modify the state.
        if (isFirstExitFromPeeking) {
            mHasExitedPeeking = true;
        } else if (isFirstExitFromExpanded) {
            mHasExitedExpanded = true;
        } else if (isFirstExitFromMaximized) {
            mHasExitedMaximized = true;
        }

        if (toState == PanelState.EXPANDED) {
            mHasExpanded = true;
        } else if (toState == PanelState.MAXIMIZED) {
            mHasMaximized = true;
        }
        if (reason == StateChangeReason.SERP_NAVIGATION) {
            mIsSerpNavigation = true;
        }

        if (isEndingSearch) {
            mDidSearchInvolvePromo = false;
            mWasSearchContentViewSeen = false;
            mHasExpanded = false;
            mHasMaximized = false;
            mHasExitedPeeking = false;
            mHasExitedExpanded = false;
            mHasExitedMaximized = false;
            mIsSerpNavigation = false;
            mWasContextualCardsDataShown = false;
            mWasQuickActionShown = false;
            mQuickActionCategory = QuickActionCategory.NONE;
            mCardTag = ResolvedSearchTerm.CardTag.CT_NONE;
            mWasQuickActionClicked = false;
            mWasAnyHeuristicSatisfiedOnPanelShow = false;
            mPanelTriggerTimeFromTapNs = 0;
        }
    }

    /**
     * Sets that the contextual search involved the promo.
     */
    public void setDidSearchInvolvePromo() {
        mDidSearchInvolvePromo = true;
    }

    /**
     * Sets that the Search Content View was seen.
     */
    public void setWasSearchContentViewSeen() {
        mWasSearchContentViewSeen = true;
        mWasPanelOpenedBeyondPeek = true;
        mPanelOpenedBeyondPeekTimeNs = System.nanoTime();
        mPanelOpenedBeyondPeekDurationMs = 0;
    }

    /**
     * Sets whether the promo is active.
     */
    public void setIsPromoActive(boolean shown) {
        mIsPromoActive = shown;
    }

    /**
     * @param wasContextualCardsDataShown Whether Contextual Cards data was shown in the Contextual
     *                                    Search Bar.
     */
    public void setWasContextualCardsDataShown(
            boolean wasContextualCardsDataShown, @ResolvedSearchTerm.CardTag int cardTag) {
        mWasContextualCardsDataShown = wasContextualCardsDataShown;
        mCardTag = cardTag;
    }

    /**
     * @param wasQuickActionShown Whether a quick action was shown in the Contextual Search Bar.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     */
    public void setWasQuickActionShown(boolean wasQuickActionShown, int quickActionCategory) {
        mWasQuickActionShown = wasQuickActionShown;
        if (mWasQuickActionShown) mQuickActionCategory = quickActionCategory;
    }

    /**
     * Sets |mWasQuickActionClicked| to true.
     */
    public void setWasQuickActionClicked() {
        mWasQuickActionClicked = true;
    }

    /**
     * Should be called when the panel first starts showing due to a tap.
     */
    public void onPanelTriggeredFromTap() {
        mPanelTriggerTimeFromTapNs = System.nanoTime();
    }

    /**
     * @param selection The text that is selected when a selection is established.
     */
    public void onSelectionEstablished(String selection) {
        mSelectionLength = selection.length();
    }

    /**
     * Called to record the time when a search request started, for resolve and prefetch timing.
     */
    public void onSearchRequestStarted() {
        mSearchRequestStartTimeNs = System.nanoTime();
    }

    /**
     * Called when a Search Term has been resolved.
     */
    public void onSearchTermResolved() {
        long durationMs = (System.nanoTime() - mSearchRequestStartTimeNs)
                / TimeUtils.NANOSECONDS_PER_MILLISECOND;
        ContextualSearchUma.logSearchTermResolutionDuration(durationMs);
    }

    /**
     * Called after the panel has navigated to prefetched Search Results.
     * This is the point where the search result starts to render in the panel.
     */
    public void onPanelNavigatedToPrefetchedSearch(boolean didResolve) {
        long durationMs = (System.nanoTime() - mSearchRequestStartTimeNs)
                / TimeUtils.NANOSECONDS_PER_MILLISECOND;
        ContextualSearchUma.logPrefetchedSearchNavigatedDuration(durationMs, didResolve);
    }

    /**
     * Sets the experiments to log with results seen.
     * @param resultsSeenExperiments The experiments to log when the panel results are known.
     */
    public void setResultsSeenExperiments(ContextualSearchHeuristics resultsSeenExperiments) {
        mResultsSeenExperiments = resultsSeenExperiments;
    }

    /**
     * Sets up logging through Ranker for outcomes.
     * @param interactionRecorder The {@link ContextualSearchInteractionRecorder} currently being
     * used to measure to recorder user interaction outcomes.
     */
    public void setInteractionRecorder(ContextualSearchInteractionRecorder interactionRecorder) {
        mInteractionRecorder = interactionRecorder;
        mAreOutcomesValid = false;
    }

    /**
     * Writes all the outcome features to the Interaction Recorder and resets it.
     */
    public void writeInteractionOutcomesAndReset() {
        if (mInteractionRecorder != null && mWasActivatedByTap && mAreOutcomesValid) {
            // Tell Ranker about the primary outcome.
            mInteractionRecorder.logOutcome(
                    ContextualSearchInteractionRecorder.Feature.OUTCOME_WAS_PANEL_OPENED,
                    mWasSearchContentViewSeen);
            ContextualSearchUma.logRankerInference(mWasSearchContentViewSeen,
                    mInteractionRecorder.getPredictionForTapSuppression());
            mInteractionRecorder.logOutcome(
                    ContextualSearchInteractionRecorder.Feature.OUTCOME_WAS_CARDS_DATA_SHOWN,
                    mWasContextualCardsDataShown);
            if (mWasQuickActionShown) {
                mInteractionRecorder.logOutcome(ContextualSearchInteractionRecorder.Feature
                                                        .OUTCOME_WAS_QUICK_ACTION_CLICKED,
                        mWasQuickActionClicked);
            }
            if (mResultsSeenExperiments != null) {
                mResultsSeenExperiments.logRankerTapSuppressionOutcome(mInteractionRecorder);
            }
            mInteractionRecorder.writeLogAndReset();
            mInteractionRecorder = null;
        }
    }

    /**
     * Determine whether a new contextual search is starting.
     * @param toState The contextual search state that will be transitioned to.
     * @param reason The reason for the search state transition.
     * @return Whether a new contextual search is starting.
     */
    private boolean isStartingNewContextualSearch(
            @PanelState int toState, @StateChangeReason int reason) {
        return toState == PanelState.PEEKED
                && (reason == StateChangeReason.TEXT_SELECT_TAP
                        || reason == StateChangeReason.TEXT_SELECT_LONG_PRESS);
    }

    /**
     * Determine whether a contextual search is ending.
     * @param fromState The contextual search state that will be transitioned from.
     * @param toState The contextual search state that will be transitioned to.
     * @param isStartingSearch Whether a new contextual search is starting.
     * @return Whether a contextual search is ending.
     */
    private boolean isEndingContextualSearch(
            @PanelState int fromState, @PanelState int toState, boolean isStartingSearch) {
        return isOngoingContextualSearch(fromState)
                && (toState == PanelState.CLOSED || isStartingSearch);
    }

    /**
     * @param fromState The state the panel is transitioning from.
     * @return Whether there is an ongoing contextual search.
     */
    private boolean isOngoingContextualSearch(@PanelState int fromState) {
        return fromState != PanelState.UNDEFINED && fromState != PanelState.CLOSED;
    }
}
