// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.sync.SyncService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Pattern;

/**
 * Centralizes UMA data collection for Contextual Search. All calls must be made from the UI thread.
 */
public class ContextualSearchUma {
    // Constants to use for the original selection gesture
    private static final boolean LONG_PRESS = false;
    private static final boolean TAP = true;

    /** A pattern to determine if text contains any whitespace. */
    private static final Pattern CONTAINS_WHITESPACE_PATTERN = Pattern.compile("\\s");

    // Constants with ContextualSearchPreferenceState in enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({ContextualSearchPreference.UNINITIALIZED, ContextualSearchPreference.ENABLED,
            ContextualSearchPreference.DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextualSearchPreference {
        int UNINITIALIZED = 0;
        int ENABLED = 1;
        int DISABLED = 2;
        int NUM_ENTRIES = 3;
    }

    // Constants used to log UMA "enum" histograms about whether search results were seen.
    @IntDef({Results.SEEN, Results.NOT_SEEN})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Results {
        int SEEN = 0;
        int NOT_SEEN = 1;
        int NUM_ENTRIES = 2;
    }

    // Constants used to log UMA "enum" histograms about whether the selection is valid.
    @IntDef({Selection.VALID, Selection.INVALID})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Selection {
        int VALID = 0;
        int INVALID = 1;
        int NUM_ENTRIES = 2;
    }

    // Constants used to log UMA "enum" histograms about a request's outcome.
    @IntDef({Request.NOT_FAILED, Request.FAILED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Request {
        int NOT_FAILED = 0;
        int FAILED = 1;
        int NUM_ENTRIES = 2;
    }

    // Constants used to log UMA "enum" histograms with details about whether search results
    // were seen, and what the original triggering gesture was.
    @IntDef({ResultsByGesture.SEEN_FROM_TAP, ResultsByGesture.NOT_SEEN_FROM_TAP,
            ResultsByGesture.SEEN_FROM_LONG_PRESS, ResultsByGesture.NOT_SEEN_FROM_LONG_PRESS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ResultsByGesture {
        int SEEN_FROM_TAP = 0;
        int NOT_SEEN_FROM_TAP = 1;
        int SEEN_FROM_LONG_PRESS = 2;
        int NOT_SEEN_FROM_LONG_PRESS = 3;
        int NUM_ENTRIES = 4;
    }

    // Constants used to log UMA "enum" histograms with details about whether search results
    // were seen, and whether any existing tap suppression heuristics were satisfied.
    @IntDef({ResultsBySuppression.SEEN_SUPPRESSION_HEURSTIC_SATISFIED,
            ResultsBySuppression.NOT_SEEN_SUPPRESSION_HEURSTIC_SATISFIED,
            ResultsBySuppression.SEEN_SUPPRESSION_HEURSTIC_NOT_SATISFIED,
            ResultsBySuppression.NOT_SEEN_SUPPRESSION_HEURSTIC_NOT_SATISFIED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ResultsBySuppression {
        int SEEN_SUPPRESSION_HEURSTIC_SATISFIED = 0;
        int NOT_SEEN_SUPPRESSION_HEURSTIC_SATISFIED = 1;
        int SEEN_SUPPRESSION_HEURSTIC_NOT_SATISFIED = 2;
        int NOT_SEEN_SUPPRESSION_HEURSTIC_NOT_SATISFIED = 3;
        int NUM_ENTRIES = 4;
    }

    // Constants used to log UMA "enum" histograms for Quick Answers.
    @IntDef({QuickAnswerSeen.ACTIVATED_WAS_AN_ANSWER_SEEN,
            QuickAnswerSeen.ACTIVATED_WAS_AN_ANSWER_NOT_SEEN,
            QuickAnswerSeen.ACTIVATED_NOT_AN_ANSWER_SEEN,
            QuickAnswerSeen.ACTIVATED_NOT_AN_ANSWER_NOT_SEEN, QuickAnswerSeen.NOT_ACTIVATED_SEEN,
            QuickAnswerSeen.NOT_ACTIVATED_NOT_SEEN})
    @Retention(RetentionPolicy.SOURCE)
    private @interface QuickAnswerSeen {
        int ACTIVATED_WAS_AN_ANSWER_SEEN = 0;
        int ACTIVATED_WAS_AN_ANSWER_NOT_SEEN = 1;
        int ACTIVATED_NOT_AN_ANSWER_SEEN = 2;
        int ACTIVATED_NOT_AN_ANSWER_NOT_SEEN = 3;
        int NOT_ACTIVATED_SEEN = 4;
        int NOT_ACTIVATED_NOT_SEEN = 5;
        int NUM_ENTRIES = 6;
    }

    // Constants for quick action intent resolution histogram.
    @IntDef({QuickActionResolve.FAILED, QuickActionResolve.SINGLE, QuickActionResolve.MULTIPLE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface QuickActionResolve {
        int FAILED = 0;
        int SINGLE = 1;
        int MULTIPLE = 2;
        int NUM_ENTRIES = 3;
    }

    // Enums for the Counted.Event histogram designed to match the Rasta queries(event) metric.
    @IntDef({CountedEvent.UNINTELLIGENT_COUNTED, CountedEvent.INTELLIGENT_COUNTED,
            CountedEvent.INTELLIGENT_NOT_COUNTED, CountedEvent.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    private @interface CountedEvent {
        int UNINTELLIGENT_COUNTED = 0;
        int INTELLIGENT_COUNTED = 1;
        int INTELLIGENT_NOT_COUNTED = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Key used in maps from {state, reason} to state entry (exit) logging code.
     */
    static class StateChangeKey {
        final @PanelState int mState;
        final @StateChangeReason int mReason;
        final int mHashCode;

        StateChangeKey(@PanelState int state, @StateChangeReason int reason) {
            mState = state;
            mReason = reason;
            mHashCode = 31 * state + reason;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof StateChangeKey)) return false;
            if (obj == this) return true;
            StateChangeKey other = (StateChangeKey) obj;
            return mState == other.mState && mReason == other.mReason;
        }

        @Override
        public int hashCode() {
            return mHashCode;
        }
    }

    // TODO(donnd): switch from using Maps to some method that does not require creation of a key.

    // "Seen by gesture" code map: logged on first exit from expanded panel, or promo,
    // broken down by gesture.
    private static final Map<Pair<Boolean, Boolean>, Integer> SEEN_BY_GESTURE_CODES;
    static {
        final boolean unseen = false;
        final boolean seen = true;
        Map<Pair<Boolean, Boolean>, Integer> codes = new HashMap<Pair<Boolean, Boolean>, Integer>();
        codes.put(new Pair<Boolean, Boolean>(seen, TAP), ResultsByGesture.SEEN_FROM_TAP);
        codes.put(new Pair<Boolean, Boolean>(unseen, TAP), ResultsByGesture.NOT_SEEN_FROM_TAP);
        codes.put(new Pair<Boolean, Boolean>(seen, LONG_PRESS),
                ResultsByGesture.SEEN_FROM_LONG_PRESS);
        codes.put(new Pair<Boolean, Boolean>(unseen, LONG_PRESS),
                ResultsByGesture.NOT_SEEN_FROM_LONG_PRESS);
        SEEN_BY_GESTURE_CODES = Collections.unmodifiableMap(codes);
    }

    /**
     * Logs the state of the Contextual Search preference. This function should be called if the
     * Contextual Search feature is active, and will track the different preference settings
     * (disabled, enabled or uninitialized). Calling more than once is fine.
     */
    public static void logPreferenceState() {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchPreferenceState",
                getPreferenceValue(), ContextualSearchPreference.NUM_ENTRIES);
    }

    /**
     * Logs the given number of promo taps remaining.  Should be called only for users that
     * are still undecided.
     * @param promoTapsRemaining The number of taps remaining (should not be negative).
     */
    public static void logPromoTapsRemaining(int promoTapsRemaining) {
        if (promoTapsRemaining >= 0) {
            RecordHistogram.recordCount1MHistogram(
                    "Search.ContextualSearchPromoTapsRemaining", promoTapsRemaining);
        }
    }

    /**
     * Logs the historic number of times that a Tap gesture triggered the peeking promo
     * for users that have never opened the panel.  This should be called periodically for
     * undecided users only.
     * @param promoTaps The historic number of taps that have caused the peeking bar for the promo,
     *        for users that have never opened the panel.
     */
    public static void logPromoTapsForNeverOpened(int promoTaps) {
        RecordHistogram.recordCount1MHistogram(
                "Search.ContextualSearchPromoTapsForNeverOpened", promoTaps);
    }

    /**
     * Logs the historic number of times that a Tap gesture triggered the peeking promo before
     * the user ever opened the panel.  This should be called periodically for all users.
     * @param promoTaps The historic number of taps that have caused the peeking bar for the promo
     *        before the first open of the panel, for all users that have ever opened the panel.
     */
    public static void logPromoTapsBeforeFirstOpen(int promoTaps) {
        RecordHistogram.recordCount1MHistogram(
                "Search.ContextualSearchPromoTapsBeforeFirstOpen", promoTaps);
    }

    /**
     * Records the total count of times the promo panel has *ever* been opened.  This should only
     * be called when the user is still undecided.
     * @param count The total historic count of times the panel has ever been opened for the
     *        current user.
     */
    public static void logPromoOpenCount(int count) {
        RecordHistogram.recordCount1MHistogram("Search.ContextualSearchPromoOpenCount", count);
    }

    /**
     * Records the total count of times the revised promo card has *ever* been opened. This should
     * only be called when the user is still undecided.
     * @param count The total historic count of times the revised promo card ever been shown.
     */
    public static void logRevisedPromoOpenCount(int count) {
        RecordHistogram.recordCount1MHistogram("Search.ContextualSearchPromoOpenCount2", count);
    }

    /**
     * Logs the number of taps that have been counted since the user last opened the panel, for
     * undecided users.
     * @param tapsSinceOpen The number of taps to log.
     */
    public static void logTapsSinceOpenForUndecided(int tapsSinceOpen) {
        RecordHistogram.recordCount1MHistogram(
                "Search.ContextualSearchTapsSinceOpenUndecided", tapsSinceOpen);
    }

    /**
     * Logs the number of taps that have been counted since the user last opened the panel, for
     * decided users.
     * @param tapsSinceOpen The number of taps to log.
     */
    public static void logTapsSinceOpenForDecided(int tapsSinceOpen) {
        RecordHistogram.recordCount1MHistogram(
                "Search.ContextualSearchTapsSinceOpenDecided", tapsSinceOpen);
    }

    /**
     * Logs changes to the Contextual Search preference, aside from those resulting from the first
     * run flow.
     * @param enabled Whether the preference is being enabled or disabled.
     */
    public static void logMainPreferenceChange(boolean enabled) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchPreferenceStateChange",
                enabled ? ContextualSearchPreference.ENABLED : ContextualSearchPreference.DISABLED,
                ContextualSearchPreference.NUM_ENTRIES);
    }

    /**
     * Logs changes to the Contextual Search privacy opt-in preference.
     * @param enabled Whether the opt-in preference is being enabled or disabled.
     */
    public static void logPrivacyOptInPreferenceChange(boolean enabled) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchPrivacyOptInPreferenceStateChange", enabled);
    }

    /**
     * Logs the user's choice for the Contextual Search Promo Card.
     * @param enabled Whether the opt-in to full privacy is being chosen.
     */
    public static void logPromoCardChoice(boolean enabled) {
        RecordHistogram.recordBooleanHistogram("Search.ContextualSearchPromoCardChoice", enabled);
    }

    /**
     * Logs the duration of a Contextual Search panel being viewed by the user.
     * @param wereResultsSeen Whether search results were seen.
     * @param isChained Whether the Contextual Search ended with the start of another.
     * @param durationMs The duration of the contextual search in milliseconds.
     */
    public static void logDuration(boolean wereResultsSeen, boolean isChained, long durationMs) {
        if (wereResultsSeen) {
            RecordHistogram.recordTimesHistogram("Search.ContextualSearchDurationSeen", durationMs);
        } else if (isChained) {
            RecordHistogram.recordTimesHistogram(
                    "Search.ContextualSearchDurationUnseenChained", durationMs);
        } else {
            RecordHistogram.recordTimesHistogram(
                    "Search.ContextualSearchDurationUnseen", durationMs);
        }
    }

    /**
     * Logs the duration from starting a search until the Search Term is resolved.
     * @param durationMs The duration to record.
     */
    public static void logSearchTermResolutionDuration(long durationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                "Search.ContextualSearchResolutionDuration", durationMs);
    }

    /**
     * Logs the duration from starting a prefetched search until the panel navigates to the results
     * and they start becoming viewable. Should be called only for searches that are prefetched.
     * @param durationMs The duration to record.
     * @param didResolve Whether a Search Term resolution was required as part of the loading.
     */
    public static void logPrefetchedSearchNavigatedDuration(long durationMs, boolean didResolve) {
        String histogramName = didResolve ? "Search.ContextualSearchResolvedSearchDuration"
                                          : "Search.ContextualSearchLiteralSearchDuration";
        RecordHistogram.recordMediumTimesHistogram(histogramName, durationMs);
    }

    /**
     * Logs the duration from opening the panel beyond peek until the panel is closed.
     * @param durationMs The duration to record.
     */
    public static void logPanelOpenDuration(long durationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                "Search.ContextualSearchPanelOpenDuration", durationMs);
    }

    /**
     * Logs a user action for the duration of viewing the panel that describes the amount of time
     * the user viewed the bar and panel overall.
     * @param durationMs The duration to record.
     */
    public static void logPanelViewDurationAction(long durationMs) {
        if (durationMs < DateUtils.SECOND_IN_MILLIS) {
            RecordUserAction.record("ContextualSearch.ViewLessThanOneSecond");
        } else if (durationMs < DateUtils.SECOND_IN_MILLIS * 3) {
            RecordUserAction.record("ContextualSearch.ViewOneToThreeSeconds");
        } else if (durationMs < DateUtils.SECOND_IN_MILLIS * 10) {
            RecordUserAction.record("ContextualSearch.ViewThreeToTenSeconds");
        } else {
            RecordUserAction.record("ContextualSearch.ViewMoreThanTenSeconds");
        }
    }

    /**
     * Logs whether the promo was seen.
     * Logs multiple histograms, with and without the original triggering gesture.
     * @param wasPanelSeen Whether the panel was seen.
     * @param wasTap Whether the gesture that originally caused the panel to show was a Tap.
     */
    public static void logPromoSeen(boolean wasPanelSeen, boolean wasTap) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchFirstRunPanelSeen",
                wasPanelSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
        logHistogramByGesture(wasPanelSeen, wasTap, "Search.ContextualSearchPromoSeenByGesture");
    }

    /**
     * Logs whether search results were seen.
     * Logs multiple histograms; with and without the original triggering gesture.
     * @param wasPanelSeen Whether the panel was seen.
     * @param wasTap Whether the gesture that originally caused the panel to show was a Tap.
     */
    public static void logResultsSeen(boolean wasPanelSeen, boolean wasTap) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchResultsSeen",
                wasPanelSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
        logHistogramByGesture(wasPanelSeen, wasTap, "Search.ContextualSearchResultsSeenByGesture");
    }

    /**
     * Logs whether search results were seen for a Tap gesture, for all users and sync-enabled
     * users. For sync-enabled users we log to a separate histogram for that sub-population in order
     * to help validate the Ranker Tap Suppression model results (since they are trained on UKM data
     * which approximately reflects this sync-enabled population).
     * @param wasPanelSeen Whether the panel was seen.
     */
    public static void logTapResultsSeen(boolean wasPanelSeen) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.Tap.ResultsSeen", wasPanelSeen);
        if (SyncService.get() != null && SyncService.get().isSyncRequested()) {
            RecordHistogram.recordBooleanHistogram(
                    "Search.ContextualSearch.Tap.SyncEnabled.ResultsSeen", wasPanelSeen);
        }
    }

    /**
     * Logs whether search results were seen for all gestures.  Recorded for all users.
     * @param wasPanelSeen Whether the panel was seen.
     */
    public static void logAllResultsSeen(boolean wasPanelSeen) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.All.ResultsSeen", wasPanelSeen);
        // Log a user action for the wasPanelSeen case. This value is used as part of a high-level
        // guiding metric, which is being migrated to user actions.
        if (wasPanelSeen) {
            RecordUserAction.record("Search.ContextualSearch.All.ResultsSeen.true");
        }
    }

    /**
     * Logs all searches that were displayed to the user in a Search Result Page in the panel.
     * @param wasRelatedSearches Whether the search was due to Related Searches (as opposed to
     *      being a regular Contextual Search query).
     */
    public static void logAllSearches(boolean wasRelatedSearches) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.All.Searches", wasRelatedSearches);
    }

    /**
     * Logs histograms designed to match the Rasta queries(event) metric.
     * One histogram summarizes all triggering of this feature and whether they were counted or not.
     * Another histogram breaks down the cases where the panel was opened by what kind of search it
     * was and whether it was counted by Rasta.
     * The intent is to filter out very brief opens of the panel in which the SRP did not load in
     * time for the user to see it or for Rasta to count it. In particular the Contextual Search
     * dynamic JavaScript that converts a prefetch into a real search needs to have loaded and
     * executed to count prefetch conversions as Searches in Rasta.
     * @param wasPanelSeen Whether the panel was opened beyond the peeking state.
     * @param wasDocumentPainted Whether the document was actually starting to render in the Content
     *         View so the user could actually see it.
     * @param wasPrefetch Whether the Search was a prefetch generated by an intelligent search.
     */
    public static void logCountedSearches(
            boolean wasPanelSeen, boolean wasDocumentPainted, boolean wasPrefetch) {
        // Get the enum for the category for those seen, and record the category of the search.
        // Default to the simplest Counted enum, which is useful for both histograms.
        @CountedEvent
        int searchEnum = CountedEvent.UNINTELLIGENT_COUNTED;
        if (wasPanelSeen) {
            // Prefetch indicates an intelligent search and might not have been counted through
            // dynamic JavaScript conversion.
            if (wasPrefetch) {
                searchEnum = wasDocumentPainted ? CountedEvent.INTELLIGENT_COUNTED
                                                : CountedEvent.INTELLIGENT_NOT_COUNTED;
            }
            RecordHistogram.recordEnumeratedHistogram(
                    "Search.ContextualSearch.Counted.Event", searchEnum, CountedEvent.NUM_ENTRIES);
        }
        boolean wasSeenAndCounted =
                wasPanelSeen && searchEnum != CountedEvent.INTELLIGENT_NOT_COUNTED;
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.Counted.Searches", wasSeenAndCounted);
    }

    /**
     * Logs a User Action for promoting the Overlay into it's own separate Tab.
     * @param isShowingRelatedSearchSerp Whether the current SERP shown in the Overlay is from
     *    Related Searches or not (just a plain Contextual Search).
     */
    public static void logTabPromotion(boolean isShowingRelatedSearchSerp) {
        if (isShowingRelatedSearchSerp) {
            RecordUserAction.record("RelatedSearches.TabPromotion");
        } else {
            RecordUserAction.record("ContextualSearch.TabPromotion");
        }
    }

    /**
     * Logs a User Action for clicking on a search result in the Search Result Page.
     * @param isShowingRelatedSearchSerp Whether the current SERP shown in the Overlay is from
     *    Related Searches or not (just a plain Contextual Search).
     */
    public static void logSerpResultClicked(boolean isShowingRelatedSearchSerp) {
        if (isShowingRelatedSearchSerp) {
            RecordUserAction.record("RelatedSearches.SerpResultClicked");
        } else {
            RecordUserAction.record("ContextualSearch.SerpResultClicked");
        }
    }

    /**
     * Logs the length of the selection in two histograms, one when results were seen and one when
     * results were not seen.
     * @param wasPanelSeen Whether the panel was seen.
     * @param selectionLength The length of the triggering selection.
     */
    public static void logSelectionLengthResultsSeen(boolean wasPanelSeen, int selectionLength) {
        if (wasPanelSeen) {
            RecordHistogram.recordSparseHistogram(
                    "Search.ContextualSearchSelectionLengthSeen", selectionLength);
        } else {
            RecordHistogram.recordSparseHistogram(
                    "Search.ContextualSearchSelectionLengthNotSeen", selectionLength);
        }
    }

    /**
     * Log whether the UX was suppressed due to the selection length.
     * @param wasSuppressed Whether showing the UX was suppressed due to selection length.
     */
    public static void logSelectionLengthSuppression(boolean wasSuppressed) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchSelectionLengthSuppression", wasSuppressed);
    }

    /**
     * Logs whether results were seen and whether any tap suppression heuristics were satisfied.
     * @param wasSearchContentViewSeen If the panel was opened.
     * @param wasAnySuppressionHeuristicSatisfied Whether any of the implemented suppression
     *                                            heuristics were satisfied.
     */
    public static void logAnyTapSuppressionHeuristicSatisfied(boolean wasSearchContentViewSeen,
            boolean wasAnySuppressionHeuristicSatisfied) {
        int code;
        if (wasAnySuppressionHeuristicSatisfied) {
            code = wasSearchContentViewSeen
                    ? ResultsBySuppression.SEEN_SUPPRESSION_HEURSTIC_SATISFIED
                    : ResultsBySuppression.NOT_SEEN_SUPPRESSION_HEURSTIC_SATISFIED;
        } else {
            code = wasSearchContentViewSeen
                    ? ResultsBySuppression.SEEN_SUPPRESSION_HEURSTIC_NOT_SATISFIED
                    : ResultsBySuppression.NOT_SEEN_SUPPRESSION_HEURSTIC_NOT_SATISFIED;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Search.ContextualSearchTapSuppressionSeen.AnyHeuristicSatisfied", code,
                ResultsBySuppression.NUM_ENTRIES);
    }

    /**
     * Logs whether a selection is valid.
     * @param isSelectionValid Whether the selection is valid.
     */
    public static void logSelectionIsValid(boolean isSelectionValid) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchSelectionValid",
                isSelectionValid ? Selection.VALID : Selection.INVALID, Selection.NUM_ENTRIES);
    }

    /**
     * Logs whether a normal priority search request failed.
     * @param isFailure Whether the request failed.
     */
    public static void logNormalPrioritySearchRequestOutcome(boolean isFailure) {
        RecordHistogram.recordEnumeratedHistogram(
                "Search.ContextualSearchNormalPrioritySearchRequestStatus",
                isFailure ? Request.FAILED : Request.NOT_FAILED, Request.NUM_ENTRIES);
    }

    /**
     * Logs whether a low priority search request failed.
     * @param isFailure Whether the request failed.
     */
    public static void logLowPrioritySearchRequestOutcome(boolean isFailure) {
        RecordHistogram.recordEnumeratedHistogram(
                "Search.ContextualSearchLowPrioritySearchRequestStatus",
                isFailure ? Request.FAILED : Request.NOT_FAILED, Request.NUM_ENTRIES);
    }

    /**
     * Logs whether a fallback search request failed.
     * @param isFailure Whether the request failed.
     */
    public static void logFallbackSearchRequestOutcome(boolean isFailure) {
        RecordHistogram.recordEnumeratedHistogram(
                "Search.ContextualSearchFallbackSearchRequestStatus",
                isFailure ? Request.FAILED : Request.NOT_FAILED, Request.NUM_ENTRIES);
    }

    /**
     * Log whether the UX was suppressed by a recent scroll.
     * @param wasSuppressed Whether showing the UX was suppressed by a recent scroll.
     */
    public static void logRecentScrollSuppression(boolean wasSuppressed) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchRecentScrollSuppression", wasSuppressed);
    }

    /**
     * Logs the duration between the panel being triggered due to a tap and the panel being
     * dismissed due to a scroll.
     * @param durationSincePanelTriggerMs The amount of time between the panel getting triggered and
     *                                    the panel being dismissed due to a scroll.
     * @param wasSearchContentViewSeen If the panel was opened.
     */
    public static void logDurationBetweenTriggerAndScroll(
            long durationSincePanelTriggerMs, boolean wasSearchContentViewSeen) {
        String histogram = wasSearchContentViewSeen
                ? "Search.ContextualSearchDurationBetweenTriggerAndScrollSeen"
                : "Search.ContextualSearchDurationBetweenTriggerAndScrollNotSeen";
        if (durationSincePanelTriggerMs < 2000) {
            RecordHistogram.recordCustomCountHistogram(
                    histogram, (int) durationSincePanelTriggerMs, 1, 2000, 200);
        }
    }

    /**
     * Logs whether a Quick Answer caption was activated, and whether it was an answer (as opposed
     * to just being informative), and whether the panel was opened anyway.
     * Logged only for Tap events.
     * @param didActivate If the Quick Answer caption was shown.
     * @param didAnswer If the caption was considered an answer (reducing the need to open the
     *        panel).
     * @param wasSearchContentViewSeen If the panel was opened.
     */
    static void logQuickAnswerSeen(
            boolean wasSearchContentViewSeen, boolean didActivate, boolean didAnswer) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchQuickAnswerSeen",
                getQuickAnswerSeenValue(didActivate, didAnswer, wasSearchContentViewSeen),
                QuickAnswerSeen.NUM_ENTRIES);
    }

    /**
     * Logs a user action for a change to the Panel state, which allows sequencing of actions.
     * @param toState The state to transition to.
     * @param reason The reason for the state transition.
     */
    public static void logPanelStateUserAction(
            @PanelState int toState, @StateChangeReason int reason) {
        switch (toState) {
            case PanelState.CLOSED:
                if (reason == StateChangeReason.BACK_PRESS) {
                    RecordUserAction.record("ContextualSearch.BackPressClose");
                } else if (reason == StateChangeReason.CLOSE_BUTTON) {
                    RecordUserAction.record("ContextualSearch.CloseButtonClose");
                } else if (reason == StateChangeReason.SWIPE || reason == StateChangeReason.FLING) {
                    RecordUserAction.record("ContextualSearch.SwipeOrFlingClose");
                } else if (reason == StateChangeReason.TAB_PROMOTION) {
                    RecordUserAction.record("ContextualSearch.TabPromotionClose");
                } else if (reason == StateChangeReason.BASE_PAGE_TAP) {
                    RecordUserAction.record("ContextualSearch.BasePageTapClose");
                } else if (reason == StateChangeReason.BASE_PAGE_SCROLL) {
                    RecordUserAction.record("ContextualSearch.BasePageScrollClose");
                } else if (reason == StateChangeReason.SEARCH_BAR_TAP) {
                    RecordUserAction.record("ContextualSearch.SearchBarTapClose");
                } else if (reason == StateChangeReason.SERP_NAVIGATION) {
                    RecordUserAction.record("ContextualSearch.NavigationClose");
                } else {
                    RecordUserAction.record("ContextualSearch.UncommonClose");
                }
                break;
            case PanelState.PEEKED:
                if (reason == StateChangeReason.TEXT_SELECT_TAP) {
                    RecordUserAction.record("ContextualSearch.TapPeek");
                } else if (reason == StateChangeReason.SWIPE || reason == StateChangeReason.FLING) {
                    RecordUserAction.record("ContextualSearch.SwipeOrFlingPeek");
                } else if (reason == StateChangeReason.TEXT_SELECT_LONG_PRESS) {
                    RecordUserAction.record("ContextualSearch.LongpressPeek");
                }
                break;
            case PanelState.EXPANDED:
                if (reason == StateChangeReason.SWIPE || reason == StateChangeReason.FLING) {
                    RecordUserAction.record("ContextualSearch.SwipeOrFlingExpand");
                } else if (reason == StateChangeReason.SEARCH_BAR_TAP) {
                    RecordUserAction.record("ContextualSearch.SearchBarTapExpand");
                }
                break;
            case PanelState.MAXIMIZED:
                if (reason == StateChangeReason.SWIPE || reason == StateChangeReason.FLING) {
                    RecordUserAction.record("ContextualSearch.SwipeOrFlingMaximize");
                } else if (reason == StateChangeReason.SERP_NAVIGATION) {
                    RecordUserAction.record("ContextualSearch.NavigationMaximize");
                }
                break;
            default:
                break;
        }
    }

    /**
     * Logs that the user established a new selection when Contextual Search is active.
     */
    public static void logSelectionEstablished() {
        RecordUserAction.record("ContextualSearch.SelectionEstablished");
    }

    /**
     * Logs that the user manually adjusted a selection when Contextual Search is active.
     * @param selection The new selection.
     */
    public static void logSelectionAdjusted(@Nullable String selection) {
        if (TextUtils.isEmpty(selection)) return;

        boolean isSingleWord = !CONTAINS_WHITESPACE_PATTERN.matcher(selection.trim()).find();
        if (isSingleWord) {
            RecordUserAction.record("ContextualSearch.ManualRefineSingleWord");
        } else {
            RecordUserAction.record("ContextualSearch.ManualRefineMultiWord");
        }
    }

    /** Logs a UserAction that the user just acknowledged the Longpress in-panel-help. */
    static void logInPanelHelpAcknowledged() {
        RecordUserAction.record("ContextualSearch.logInPanelHelpAcknowledged");
    }

    /**
     * Logs that the system automatically expanded the selection when a user triggered
     * Contextual Search on a multiword phrase that could be identified by the server.
     * @param fromTapGesture Whether the gesture that originally established the selection
     *        was Tap.
     */
    public static void logSelectionExpanded(boolean fromTapGesture) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.SelectionExpanded", fromTapGesture);
    }

    /**
     * Logs that the system sent a server request to resolve the search term.
     * @param fromTapGesture Whether the gesture that originally established the selection
     *        was Tap.
     */
    public static void logResolveRequested(boolean fromTapGesture) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.ResolveRequested", fromTapGesture);
    }

    /**
     * Logs that the system received a server response from a resolve request.
     * @param fromTapGesture Whether the gesture that originally established the selection
     *        was Tap.
     */
    public static void logResolveReceived(boolean fromTapGesture) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.ResolveReceived", fromTapGesture);
    }

    /**
     * Logs that the user needs a translation of the selection. The user may or may not actually
     * see a translation - this only logs that it's needed.
     * @param fromTapGesture Whether the gesture that originally established the selection
     *        was Tap.
     */
    public static void logTranslationNeeded(boolean fromTapGesture) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.TranslationNeeded", fromTapGesture);
    }

    /**
     * Logs a duration since the outcomes (and associated timestamp) were saved in persistent
     * storage.
     * @param durationMs The duration to log, in milliseconds.
     */
    public static void logOutcomesTimestamp(long durationMs) {
        int durationInDays = (int) (durationMs / DateUtils.DAY_IN_MILLIS);
        RecordHistogram.recordCount100Histogram(
                "Search.ContextualSearch.OutcomesDuration", durationInDays);
    }

    /**
     * Logs whether Contextual Cards data was shown. Should be logged on tap if Contextual
     * Cards integration is enabled.
     * @param shown Whether Contextual Cards data was shown in the Bar.
     */
    public static void logContextualCardsDataShown(boolean shown) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchContextualCardsIntegration.DataShown", shown);
    }

    /**
     * Logs whether a quick action intent resolved to zero, one, or many apps.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     * @param numMatchingAppsApps The number of apps that the resolved intent matched.
     */
    public static void logQuickActionIntentResolution(int quickActionCategory,
            int numMatchingAppsApps) {
        int code = numMatchingAppsApps == 0
                ? QuickActionResolve.FAILED
                : numMatchingAppsApps == 1 ? QuickActionResolve.SINGLE
                                           : QuickActionResolve.MULTIPLE;
        RecordHistogram.recordEnumeratedHistogram(
                "Search.ContextualSearchQuickActions.IntentResolution."
                        + getLabelForQuickActionCategory(quickActionCategory),
                code, QuickActionResolve.NUM_ENTRIES);
    }

    /**
     * Logs whether a quick action was shown, and the quick aciton category if a quick action was
     * shown. Should be logged on tap if Contextual Search single actions are enabled.
     * @param quickActionShown Whether a quick action was shown.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     */
    public static void logQuickActionShown(boolean quickActionShown, int quickActionCategory) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchQuickActions.Shown", quickActionShown);
        if (quickActionShown) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Search.ContextualSearchQuickActions.Category", quickActionCategory,
                    QuickActionCategory.BOUNDARY);
        }
    }

    /**
     * Logs whether results were seen when a quick action was present.
     * @param wasSeen Whether the search results were seen.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     */
    public static void logQuickActionResultsSeen(boolean wasSeen, int quickActionCategory) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchQuickActions.ResultsSeen."
                        + getLabelForQuickActionCategory(quickActionCategory),
                wasSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
    }

    /**
     * Logs whether a quick action was clicked.
     * @param wasClicked Whether the quick action was clicked
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     */
    public static void logQuickActionClicked(boolean wasClicked, int quickActionCategory) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchQuickActions.Clicked."
                        + getLabelForQuickActionCategory(quickActionCategory),
                 wasClicked);
    }

    /**
     * Logs the primary CoCa {@link CardTag} for searches where the panel contents was seen,
     * including {@codeCardTag.CT_NONE} when no card or tag, and {@codeCardTag.CT_OTHER} when it's
     * one we do not recognize.
     * @param wasSearchContentViewSeen Whether the panel was seen.
     * @param cardTagEnum The primary CoCa card Tag for the result seen.
     */
    public static void logCardTagSeen(boolean wasSearchContentViewSeen, @CardTag int cardTagEnum) {
        if (wasSearchContentViewSeen) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Search.ContextualSearch.CardTagSeen", cardTagEnum, CardTag.NUM_ENTRIES);
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Search.ContextualSearch.CardTag", cardTagEnum, CardTag.NUM_ENTRIES);
    }

    /**
     * Logs results-seen when we have a useful Ranker prediction inference.
     * @param wasPanelSeen Whether the panel was seen.
     * @param predictionKind An integer reflecting the Ranker prediction, e.g. that this is a good
     *        time to suppress triggering because the likelihood of opening the panel is relatively
     *        low.
     */
    public static void logRankerInference(
            boolean wasPanelSeen, @AssistRankerPrediction int predictionKind) {
        if (predictionKind == AssistRankerPrediction.SHOW) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Search.ContextualSearch.Ranker.NotSuppressed.ResultsSeen",
                    wasPanelSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
        } else if (predictionKind == AssistRankerPrediction.SUPPRESS) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Search.ContextualSearch.Ranker.WouldSuppress.ResultsSeen",
                    wasPanelSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
        }
    }

    /**
     * Logs Ranker's prediction of whether or not to suppress.
     * @param predictionKind An integer reflecting the Ranker prediction, e.g. that this is a good
     *        time to suppress triggering because the likelihood of opening the panel is relatively
     *        low.
     */
    public static void logRankerPrediction(@AssistRankerPrediction int predictionKind) {
        // For now we just log whether or not suppression is predicted.
        RecordHistogram.recordBooleanHistogram("Search.ContextualSearch.Ranker.Suppressed",
                predictionKind == AssistRankerPrediction.SUPPRESS);
    }

    /** Logs that Ranker recorded a set of features for training or inference. */
    public static void logRecordedFeaturesToRanker() {
        logRecordedToRanker(false);
    }

    /** Logs that Ranker recorded a set of outcomes for training or inference. */
    public static void logRecordedOutcomesToRanker() {
        logRecordedToRanker(true);
    }

    /**
     * Logs that Ranker recorded some data for training or inference.
     * @param areOutcomes Whether the data are outcomes.
     */
    private static void logRecordedToRanker(boolean areOutcomes) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.Ranker.Recorded", areOutcomes);
    }

    /**
     * Logs that features or outcomes are available to record to Ranker.
     * This data can be used to correlate with #logRecordedToRanker to validate that everything that
     * should be recorded is actually being recorded.
     * @param areOutcomes Whether the features available are outcomes.
     */
    static void logRankerFeaturesAvailable(boolean areOutcomes) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.Ranker.FeaturesAvailable", areOutcomes);
    }

    /**
     * Logs the previous enabled-state of this user before the feature was turned full-on for
     * Unified Consent (when integration is enabled).
     * @param wasPreviouslyUndecided Whether the user was previously undecided.
     */
    static void logUnifiedConsentPreviousEnabledState(boolean wasPreviouslyUndecided) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.UnifiedConsent.PreviouslyUndecided",
                wasPreviouslyUndecided);
    }

    /**
     * Logs whether a request will be throttled for Unified Consent integration, for all requests
     * regardless of whether the integration feature is enabled.  Logged multiple times per request.
     * @param isRequestThrottled Whether the current request is being throttled.
     */
    static void logUnifiedConsentThrottledRequests(boolean isRequestThrottled) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.UnifiedConsent.ThrottledRequests", isRequestThrottled);
    }

    /**
     * Logs whether this user was eligible for throttling of requests when Unified Consent
     * integration is enabled and throttling is in effect.
     * @param isThrottleEligible Whether this user is eligible to be throttled.
     */
    static void logUnifiedConsentThrottleEligible(boolean isThrottleEligible) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.UnifiedConsent.ThrottleEligible", isThrottleEligible);
    }

    /**
     * Gets the panel-seen code for the given parameters by doing a lookup in the seen-by-gesture
     * map.
     * @param wasPanelSeen Whether the panel was seen.
     * @param wasTap Whether the gesture that originally caused the panel to show was a Tap.
     * @return The code to write into a panel-seen histogram.
     */
    private static int getPanelSeenByGestureStateCode(boolean wasPanelSeen, boolean wasTap) {
        return SEEN_BY_GESTURE_CODES.get(new Pair<Boolean, Boolean>(wasPanelSeen, wasTap));
    }

    /**
     * @return The code for the Contextual Search preference.
     */
    private static int getPreferenceValue() {
        if (ContextualSearchPolicy.isContextualSearchUninitialized()) {
            return ContextualSearchPreference.UNINITIALIZED;
        } else if (ContextualSearchPolicy.isContextualSearchDisabled()) {
            return ContextualSearchPreference.DISABLED;
        }
        return ContextualSearchPreference.ENABLED;
    }

    /**
     * Gets the encode value for quick answers seen.
     * @param didActivate Whether the quick answer was shown.
     * @param didAnswer Whether the caption was a full answer, not just a hint.
     * @param wasSeen Whether the search panel was opened.
     * @return The encoded value.
     */
    private static @QuickAnswerSeen int getQuickAnswerSeenValue(
            boolean didActivate, boolean didAnswer, boolean wasSeen) {
        if (wasSeen) {
            if (didActivate) {
                return didAnswer ? QuickAnswerSeen.ACTIVATED_WAS_AN_ANSWER_SEEN
                                 : QuickAnswerSeen.ACTIVATED_NOT_AN_ANSWER_SEEN;
            } else {
                return QuickAnswerSeen.NOT_ACTIVATED_SEEN;
            }
        } else {
            if (didActivate) {
                return didAnswer ? QuickAnswerSeen.ACTIVATED_WAS_AN_ANSWER_NOT_SEEN
                                 : QuickAnswerSeen.ACTIVATED_NOT_AN_ANSWER_NOT_SEEN;
            } else {
                return QuickAnswerSeen.NOT_ACTIVATED_NOT_SEEN;
            }
        }
    }

    /**
     * Logs to a seen-by-gesture histogram of the given name.
     * @param wasPanelSeen Whether the panel was seen.
     * @param wasTap Whether the gesture that originally caused the panel to show was a Tap.
     * @param histogramName The full name of the histogram to log to.
     */
    private static void logHistogramByGesture(boolean wasPanelSeen, boolean wasTap,
            String histogramName) {
        RecordHistogram.recordEnumeratedHistogram(histogramName,
                getPanelSeenByGestureStateCode(wasPanelSeen, wasTap), ResultsByGesture.NUM_ENTRIES);
    }

    private static String getLabelForQuickActionCategory(int quickActionCategory) {
        switch(quickActionCategory) {
            case QuickActionCategory.ADDRESS:
                return "Address";
            case QuickActionCategory.EMAIL:
                return "Email";
            case QuickActionCategory.EVENT:
                return "Event";
            case QuickActionCategory.PHONE:
                return "Phone";
            case QuickActionCategory.WEBSITE:
                return "Website";
            default:
                return "None";
        }
    }
}
