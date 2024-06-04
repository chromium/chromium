// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchHeuristics;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.contextualsearch.QuickActionCategory;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * This class is responsible for all the logging triggered by activity of the {@link
 * ContextualSearchPanel}. Typically this consists of tracking user activity logging that to UMA
 * when the interaction ends as the panel is dismissed.
 */
public class ContextualSearchPanelMetrics {
    // Flags for logging.
    private boolean mDidSearchInvolvePromo;
    private boolean mWasSearchContentViewSeen;
    private boolean mIsPromoActive;
    private boolean mHasExitedPeeking;
    private boolean mHasExitedExpanded;
    private boolean mHasExitedMaximized;
    private boolean mIsSerpNavigation;
    private boolean mWasActivatedByTap;
    @ResolvedSearchTerm.CardTag private int mCardTag;
    private boolean mWasQuickActionShown;
    private int mQuickActionCategory;
    private boolean mWasQuickActionClicked;
    // Time when the panel was triggered (not reset by a chained search).
    // Panel transitions are animated so mPanelTriggerTimeNs will be less than mFirstPeekTimeNs.
    private long mPanelTriggerTimeFromTapNs;
    // Time when the panel peeks into view (not reset by a chained search).
    // Used to log total time the panel is showing (not closed).
    private long mFirstPeekTimeNs;
    // Time when a search request was started. Reset by chained searches.
    // Used to log the time it takes for a Search Result to become available.
    private long mSearchRequestStartTimeNs;
    // The current set of heuristics that should be logged with results seen when the panel closes.
    private ContextualSearchHeuristics mResultsSeenExperiments;

    /** Whether the Search was prefetched or not. */
    private boolean mWasPrefetch;

    /**
     * Whether we were able to start painting the document in the Content View so the user could
     * actually see the SRP.
     */
    private boolean mDidFirstNonEmptyPaint;

    /**
     * Log information when the panel's state has changed.
     * @param fromState The state the panel is transitioning from.
     * @param toState The state that the panel is transitioning to.
     * @param reason The reason for the state change.
     * @param profile The current {@link Profile}.
     */
    public void onPanelStateChanged(
            @PanelState int fromState,
            @PanelState int toState,
            @StateChangeReason int reason,
            Profile profile) {
        // Note: the logging within this function includes the promo, unless specifically
        // excluded.
        boolean isStartingSearch = isStartingNewContextualSearch(toState, reason);
        boolean isEndingSearch = isEndingContextualSearch(fromState, toState, isStartingSearch);
        boolean isChained = isStartingSearch && isOngoingContextualSearch(fromState);
        boolean isSameState = fromState == toState;
        boolean isFirstExitFromPeeking =
                fromState == PanelState.PEEKED
                        && !mHasExitedPeeking
                        && (!isSameState || isStartingSearch);
        boolean isFirstExitFromExpanded =
                fromState == PanelState.EXPANDED && !mHasExitedExpanded && !isSameState;
        boolean isFirstExitFromMaximized =
                fromState == PanelState.MAXIMIZED && !mHasExitedMaximized && !isSameState;

        if (isEndingSearch) {
            long panelViewDurationMs =
                    (System.nanoTime() - mFirstPeekTimeNs) / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            ContextualSearchUma.logPanelViewDurationAction(panelViewDurationMs);
            if (!mIsPromoActive) {
                ContextualSearchUma.logResultsSeen(mWasSearchContentViewSeen, mWasActivatedByTap);
            }

            ContextualSearchUma.logCardTagSeen(mWasSearchContentViewSeen, mCardTag);
            if (mWasQuickActionShown) {
                ContextualSearchUma.logQuickActionResultsSeen(
                        mWasSearchContentViewSeen, mQuickActionCategory);
                ContextualSearchUma.logQuickActionClicked(
                        mWasQuickActionClicked, mQuickActionCategory);
            }

            if (mResultsSeenExperiments != null) {
                mResultsSeenExperiments.logResultsSeen(
                        mWasSearchContentViewSeen, mWasActivatedByTap);
                if (!isChained) mResultsSeenExperiments = null;
            }

            if (mWasActivatedByTap) {
                ContextualSearchUma.logTapResultsSeen(mWasSearchContentViewSeen, profile);
            }
            ContextualSearchUma.logAllResultsSeen(mWasSearchContentViewSeen);
            if (mWasSearchContentViewSeen) {
                ContextualSearchUma.logAllSearches(/* wasRelatedSearches= */ false);
            }
            ContextualSearchUma.logCountedSearches(
                    mWasSearchContentViewSeen, mDidFirstNonEmptyPaint, mWasPrefetch);
        }

        if (isStartingSearch) {
            mFirstPeekTimeNs = System.nanoTime();
            mWasActivatedByTap = reason == StateChangeReason.TEXT_SELECT_TAP;
        }

        @StateChangeReason
        int reasonForLogging = mIsSerpNavigation ? StateChangeReason.SERP_NAVIGATION : reason;
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

        if (reason == StateChangeReason.SERP_NAVIGATION) {
            mIsSerpNavigation = true;
        }

        if (isEndingSearch) {
            mDidSearchInvolvePromo = false;
            mWasSearchContentViewSeen = false;
            mHasExitedPeeking = false;
            mHasExitedExpanded = false;
            mHasExitedMaximized = false;
            mIsSerpNavigation = false;
            mWasQuickActionShown = false;
            mQuickActionCategory = QuickActionCategory.NONE;
            mCardTag = ResolvedSearchTerm.CardTag.CT_NONE;
            mWasQuickActionClicked = false;
            mPanelTriggerTimeFromTapNs = 0;
            mDidFirstNonEmptyPaint = false;
            mWasPrefetch = false;
        }
    }

    /** Sets that the contextual search involved the promo. */
    public void setDidSearchInvolvePromo() {
        mDidSearchInvolvePromo = true;
    }

    /** Sets that the Search Content View was seen. */
    public void setWasSearchContentViewSeen() {
        mWasSearchContentViewSeen = true;
    }

    /** Sets whether the promo is active. */
    public void setIsPromoActive(boolean shown) {
        mIsPromoActive = shown;
    }

    /**
     * @param cardTag The indicator tag for the kind of card shown.
     */
    public void setCardShown(@ResolvedSearchTerm.CardTag int cardTag) {
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

    /** Sets |mWasQuickActionClicked| to true. */
    public void setWasQuickActionClicked() {
        mWasQuickActionClicked = true;
    }

    /** Should be called when the panel first starts showing due to a tap. */
    public void onPanelTriggeredFromTap() {
        mPanelTriggerTimeFromTapNs = System.nanoTime();
    }

    /** Called to record the time when a search request started, for resolve and prefetch timing. */
    public void onSearchRequestStarted() {
        mSearchRequestStartTimeNs = System.nanoTime();
    }

    /**
     * Notifies that we were able to start painting the document in the Content View so the user can
     * actually see the SRP.
     * @param didPrefetch Whether this was a prefetched SRP.
     */
    public void onFirstNonEmptyPaint(boolean didPrefetch) {
        mDidFirstNonEmptyPaint = true;
        mWasPrefetch = didPrefetch;
    }

    /**
     * Sets the experiments to log with results seen.
     * @param resultsSeenExperiments The experiments to log when the panel results are known.
     */
    public void setResultsSeenExperiments(ContextualSearchHeuristics resultsSeenExperiments) {
        mResultsSeenExperiments = resultsSeenExperiments;
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
