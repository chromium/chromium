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
import org.chromium.chrome.browser.sync.ProfileSyncService;

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

    // Constants used to log UMA "enum" histograms about the Contextual Search's preference state.
    @IntDef({Preference.UNINITIALIZED, Preference.ENABLED, Preference.DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Preference {
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

    // Constants used to log UMA "enum" histograms about the panel's state transitions.
    // Entry code: first entry into CLOSED.
    @IntDef({EnterClosedFrom.OTHER, EnterClosedFrom.PEEKED_BACK_PRESS,
            EnterClosedFrom.PEEKED_BASE_PAGE_SCROLL, EnterClosedFrom.PEEKED_TEXT_SELECT_TAP,
            EnterClosedFrom.EXPANDED_BACK_PRESS, EnterClosedFrom.EXPANDED_BASE_PAGE_TAP,
            EnterClosedFrom.EXPANDED_FLING, EnterClosedFrom.MAXIMIZED_BACK_PRESS,
            EnterClosedFrom.MAXIMIZED_FLING, EnterClosedFrom.MAXIMIZED_TAB_PROMOTION,
            EnterClosedFrom.MAXIMIZED_SERP_NAVIGATION})
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnterClosedFrom {
        int OTHER = 0;
        int PEEKED_BACK_PRESS = 1;
        int PEEKED_BASE_PAGE_SCROLL = 2;
        int PEEKED_TEXT_SELECT_TAP = 3;
        int EXPANDED_BACK_PRESS = 4;
        int EXPANDED_BASE_PAGE_TAP = 5;
        int EXPANDED_FLING = 6;
        int MAXIMIZED_BACK_PRESS = 7;
        int MAXIMIZED_FLING = 8;
        int MAXIMIZED_TAB_PROMOTION = 9;
        int MAXIMIZED_SERP_NAVIGATION = 10;
        int NUM_ENTRIES = 11;
    }

    // Entry code: first entry into PEEKED.
    @IntDef({EnterPeekedFrom.OTHER, EnterPeekedFrom.CLOSED_TEXT_SELECT_TAP,
            EnterPeekedFrom.CLOSED_EXT_SELECT_LONG_PRESS, EnterPeekedFrom.PEEKED_TEXT_SELECT_TAP,
            EnterPeekedFrom.PEEKED_TEXT_SELECT_LONG_PRESS, EnterPeekedFrom.EXPANDED_SEARCH_BAR_TAP,
            EnterPeekedFrom.EXPANDED_SWIPE, EnterPeekedFrom.EXPANDED_FLING,
            EnterPeekedFrom.MAXIMIZED_SWIPE, EnterPeekedFrom.MAXIMIZED_FLING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnterPeekedFrom {
        int OTHER = 0;
        int CLOSED_TEXT_SELECT_TAP = 1;
        int CLOSED_EXT_SELECT_LONG_PRESS = 2;
        int PEEKED_TEXT_SELECT_TAP = 3;
        int PEEKED_TEXT_SELECT_LONG_PRESS = 4;
        int EXPANDED_SEARCH_BAR_TAP = 5;
        int EXPANDED_SWIPE = 6;
        int EXPANDED_FLING = 7;
        int MAXIMIZED_SWIPE = 8;
        int MAXIMIZED_FLING = 9;
        int NUM_ENTRIES = 10;
    }

    // Entry code: first entry into EXPANDED.
    @IntDef({EnterExpandedFrom.OTHER, EnterExpandedFrom.PEEKED_SEARCH_BAR_TAP,
            EnterExpandedFrom.PEEKED_SWIPE, EnterExpandedFrom.PEEKED_FLING,
            EnterExpandedFrom.MAXIMIZED_SWIPE, EnterExpandedFrom.MAXIMIZED_FLING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnterExpandedFrom {
        int OTHER = 0;
        int PEEKED_SEARCH_BAR_TAP = 1;
        int PEEKED_SWIPE = 2;
        int PEEKED_FLING = 3;
        int MAXIMIZED_SWIPE = 4;
        int MAXIMIZED_FLING = 5;
        int NUM_ENTRIES = 6;
    }

    // Entry code: first entry into MAXIMIZED.
    @IntDef({EnterMaximizedFrom.OTHER, EnterMaximizedFrom.PEEKED_SWIPE,
            EnterMaximizedFrom.PEEKED_FLING, EnterMaximizedFrom.EXPANDED_SWIPE,
            EnterMaximizedFrom.EXPANDED_FLING, EnterMaximizedFrom.EXPANDED_SERP_NAVIGATION})
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnterMaximizedFrom {
        int OTHER = 0;
        int PEEKED_SWIPE = 1;
        int PEEKED_FLING = 2;
        int EXPANDED_SWIPE = 3;
        int EXPANDED_FLING = 4;
        int EXPANDED_SERP_NAVIGATION = 5;
        int NUM_ENTRIES = 6;
    }

    // Exit code: first exit from CLOSED (or UNDEFINED).
    @IntDef({ExitClosedTo.OTHER, ExitClosedTo.PEEKED_TEXT_SELECT_TAP,
            ExitClosedTo.PEEKED_TEXT_SELECT_LONG_PRESS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ExitClosedTo {
        int OTHER = 0;
        int PEEKED_TEXT_SELECT_TAP = 1;
        int PEEKED_TEXT_SELECT_LONG_PRESS = 2;
        int NUM_ENTRIES = 3;
    }

    // Exit code: first exit from PEEKED.
    @IntDef({ExitPeekedTo.OTHER, ExitPeekedTo.CLOSED_BACK_PRESS,
            ExitPeekedTo.CLOSED_BASE_PAGE_SCROLL, ExitPeekedTo.CLOSED_TEXT_SELECT_TAP,
            ExitPeekedTo.PEEKED_TEXT_SELECT_TAP, ExitPeekedTo.PEEKED_TEXT_SELECT_LONG_PRESS,
            ExitPeekedTo.EXPANDED_SEARCH_BAR_TAP, ExitPeekedTo.EXPANDED_SWIPE,
            ExitPeekedTo.EXPANDED_FLING, ExitPeekedTo.MAXIMIZED_SWIPE,
            ExitPeekedTo.MAXIMIZED_FLING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ExitPeekedTo {
        int OTHER = 0;
        int CLOSED_BACK_PRESS = 1;
        int CLOSED_BASE_PAGE_SCROLL = 2;
        int CLOSED_TEXT_SELECT_TAP = 3;
        int PEEKED_TEXT_SELECT_TAP = 4;
        int PEEKED_TEXT_SELECT_LONG_PRESS = 5;
        int EXPANDED_SEARCH_BAR_TAP = 6;
        int EXPANDED_SWIPE = 7;
        int EXPANDED_FLING = 8;
        int MAXIMIZED_SWIPE = 9;
        int MAXIMIZED_FLING = 10;
        int NUM_ENTRIES = 11;
    }

    // Exit code: first exit from EXPANDED.
    @IntDef({ExitExpandedTo.OTHER, ExitExpandedTo.CLOSED_BACK_PRESS,
            ExitExpandedTo.CLOSED_BASE_PAGE_TAP, ExitExpandedTo.CLOSED_FLING,
            ExitExpandedTo.PEEKED_SEARCH_BAR_TAP, ExitExpandedTo.PEEKED_SWIPE,
            ExitExpandedTo.PEEKED_FLING, ExitExpandedTo.MAXIMIZED_SWIPE,
            ExitExpandedTo.MAXIMIZED_FLING, ExitExpandedTo.MAXIMIZED_SERP_NAVIGATION})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ExitExpandedTo {
        int OTHER = 0;
        int CLOSED_BACK_PRESS = 1;
        int CLOSED_BASE_PAGE_TAP = 2;
        int CLOSED_FLING = 3;
        int PEEKED_SEARCH_BAR_TAP = 4;
        int PEEKED_SWIPE = 5;
        int PEEKED_FLING = 6;
        int MAXIMIZED_SWIPE = 7;
        int MAXIMIZED_FLING = 8;
        int MAXIMIZED_SERP_NAVIGATION = 9;
        int NUM_ENTRIES = 10;
    }

    // Exit code: first exit from MAXIMIZED.
    @IntDef({ExitMaximizedTo.OTHER, ExitMaximizedTo.CLOSED_BACK_PRESS, ExitMaximizedTo.CLOSED_FLING,
            ExitMaximizedTo.CLOSED_TAB_PROMOTION, ExitMaximizedTo.CLOSED_SERP_NAVIGATION,
            ExitMaximizedTo.PEEKED_SWIPE, ExitMaximizedTo.PEEKED_FLING,
            ExitMaximizedTo.EXPANDED_SWIPE, ExitMaximizedTo.EXPANDED_FLING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ExitMaximizedTo {
        int OTHER = 0;
        int CLOSED_BACK_PRESS = 1;
        int CLOSED_FLING = 2;
        int CLOSED_TAB_PROMOTION = 3;
        int CLOSED_SERP_NAVIGATION = 4;
        int PEEKED_SWIPE = 5;
        int PEEKED_FLING = 6;
        int EXPANDED_SWIPE = 7;
        int EXPANDED_FLING = 8;
        int NUM_ENTRIES = 9;
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

    // Constants used to log UMA "enum" histograms with details about whether search results
    // were seen, and what the original triggering gesture was.
    @IntDef({Promo.ENABLED_FROM_TAP, Promo.DISABLED_FROM_TAP, Promo.UNDECIDED_FROM_TAP,
            Promo.ENABLED_FROM_LONG_PRESS, Promo.DISABLED_FROM_LONG_PRESS,
            Promo.UNDECIDED_FROM_LONG_PRESS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Promo {
        int ENABLED_FROM_TAP = 0;
        int DISABLED_FROM_TAP = 1;
        int UNDECIDED_FROM_TAP = 2;
        int ENABLED_FROM_LONG_PRESS = 3;
        int DISABLED_FROM_LONG_PRESS = 4;
        int UNDECIDED_FROM_LONG_PRESS = 5;
        int NUM_ENTRIES = 6;
    }

    // Constants used to log UMA "enum" histograms for HTTP / HTTPS.
    @IntDef({Protocol.IS_HTTP, Protocol.NOT_HTTP})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Protocol {
        int IS_HTTP = 0;
        int NOT_HTTP = 1;
        int NUM_ENTRIES = 2;
    }

    // Constants used to log UMA "enum" histograms for single / multi-word.
    @IntDef({ResolvedGranularity.SINGLE_WORD, ResolvedGranularity.MULTI_WORD})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ResolvedGranularity {
        int SINGLE_WORD = 0;
        int MULTI_WORD = 1;
        int NUM_ENTRIES = 2;
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

    // Constants for "Bar Overlap" with triggering gesture, and whether the results were seen.
    @IntDef({BarOverlapResults.BAR_OVERLAP_RESULTS_SEEN_FROM_TAP,
            BarOverlapResults.BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_TAP,
            BarOverlapResults.NO_BAR_OVERLAP_RESULTS_SEEN_FROM_TAP,
            BarOverlapResults.NO_BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_TAP,
            BarOverlapResults.BAR_OVERLAP_RESULTS_SEEN_FROM_LONG_PRESS,
            BarOverlapResults.BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_LONG_PRESS,
            BarOverlapResults.NO_BAR_OVERLAP_RESULTS_SEEN_FROM_LONG_PRESS,
            BarOverlapResults.NO_BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_LONG_PRESS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface BarOverlapResults {
        int BAR_OVERLAP_RESULTS_SEEN_FROM_TAP = 0;
        int BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_TAP = 1;
        int NO_BAR_OVERLAP_RESULTS_SEEN_FROM_TAP = 2;
        int NO_BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_TAP = 3;
        int BAR_OVERLAP_RESULTS_SEEN_FROM_LONG_PRESS = 4;
        int BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_LONG_PRESS = 5;
        int NO_BAR_OVERLAP_RESULTS_SEEN_FROM_LONG_PRESS = 6;
        int NO_BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_LONG_PRESS = 7;
        int NUM_ENTRIES = 8;
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

    // Constants for user permissions histogram.
    @IntDef({
            Permissions.SEND_NOTHING,
            Permissions.SEND_URL,
            Permissions.SEND_CONTENT,
            Permissions.SEND_URL_AND_CONTENT,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface Permissions {
        int SEND_NOTHING = 0;
        int SEND_URL = 1;
        int SEND_CONTENT = 2;
        int SEND_URL_AND_CONTENT = 3;
        int NUM_ENTRIES = 4;
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

    // Entry code map: first entry into CLOSED.
    private static final Map<StateChangeKey, Integer> ENTER_CLOSED_STATE_CHANGE_CODES;
    static {
        Map<StateChangeKey, Integer> codes = new HashMap<StateChangeKey, Integer>();
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.BACK_PRESS),
                EnterClosedFrom.PEEKED_BACK_PRESS);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.BASE_PAGE_SCROLL),
                EnterClosedFrom.PEEKED_BASE_PAGE_SCROLL);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.TEXT_SELECT_TAP),
                EnterClosedFrom.PEEKED_TEXT_SELECT_TAP);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.BACK_PRESS),
                EnterClosedFrom.EXPANDED_BACK_PRESS);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.BASE_PAGE_TAP),
                EnterClosedFrom.EXPANDED_BASE_PAGE_TAP);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.FLING),
                EnterClosedFrom.EXPANDED_FLING);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.BACK_PRESS),
                EnterClosedFrom.MAXIMIZED_BACK_PRESS);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.FLING),
                EnterClosedFrom.MAXIMIZED_FLING);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.TAB_PROMOTION),
                EnterClosedFrom.MAXIMIZED_TAB_PROMOTION);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.SERP_NAVIGATION),
                EnterClosedFrom.MAXIMIZED_SERP_NAVIGATION);
        ENTER_CLOSED_STATE_CHANGE_CODES = Collections.unmodifiableMap(codes);
    }

    // Entry code map: first entry into PEEKED.
    private static final Map<StateChangeKey, Integer> ENTER_PEEKED_STATE_CHANGE_CODES;
    static {
        Map<StateChangeKey, Integer> codes = new HashMap<StateChangeKey, Integer>();
        // Note: we don't distinguish entering PEEKED from UNDEFINED / CLOSED.
        codes.put(new StateChangeKey(PanelState.UNDEFINED, StateChangeReason.TEXT_SELECT_TAP),
                EnterPeekedFrom.CLOSED_TEXT_SELECT_TAP);
        codes.put(
                new StateChangeKey(PanelState.UNDEFINED, StateChangeReason.TEXT_SELECT_LONG_PRESS),
                EnterPeekedFrom.CLOSED_EXT_SELECT_LONG_PRESS);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.TEXT_SELECT_TAP),
                EnterPeekedFrom.CLOSED_TEXT_SELECT_TAP);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.TEXT_SELECT_LONG_PRESS),
                EnterPeekedFrom.CLOSED_EXT_SELECT_LONG_PRESS);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.TEXT_SELECT_TAP),
                EnterPeekedFrom.PEEKED_TEXT_SELECT_TAP);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.TEXT_SELECT_LONG_PRESS),
                EnterPeekedFrom.PEEKED_TEXT_SELECT_LONG_PRESS);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.SEARCH_BAR_TAP),
                EnterPeekedFrom.EXPANDED_SEARCH_BAR_TAP);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.SWIPE),
                EnterPeekedFrom.EXPANDED_SWIPE);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.FLING),
                EnterPeekedFrom.EXPANDED_FLING);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.SWIPE),
                EnterPeekedFrom.MAXIMIZED_SWIPE);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.FLING),
                EnterPeekedFrom.MAXIMIZED_FLING);
        ENTER_PEEKED_STATE_CHANGE_CODES = Collections.unmodifiableMap(codes);
    }

    // Entry code map: first entry into EXPANDED.
    private static final Map<StateChangeKey, Integer> ENTER_EXPANDED_STATE_CHANGE_CODES;
    static {
        Map<StateChangeKey, Integer> codes = new HashMap<StateChangeKey, Integer>();
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.SEARCH_BAR_TAP),
                EnterExpandedFrom.PEEKED_SEARCH_BAR_TAP);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.SWIPE),
                EnterExpandedFrom.PEEKED_SWIPE);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.FLING),
                EnterExpandedFrom.PEEKED_FLING);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.SWIPE),
                EnterExpandedFrom.MAXIMIZED_SWIPE);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.FLING),
                EnterExpandedFrom.MAXIMIZED_FLING);
        ENTER_EXPANDED_STATE_CHANGE_CODES = Collections.unmodifiableMap(codes);
    }

    // Entry code map: first entry into MAXIMIZED.
    private static final Map<StateChangeKey, Integer> ENTER_MAXIMIZED_STATE_CHANGE_CODES;
    static {
        Map<StateChangeKey, Integer> codes = new HashMap<StateChangeKey, Integer>();
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.SWIPE),
                EnterMaximizedFrom.PEEKED_SWIPE);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.FLING),
                EnterMaximizedFrom.PEEKED_FLING);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.SWIPE),
                EnterMaximizedFrom.EXPANDED_SWIPE);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.FLING),
                EnterMaximizedFrom.EXPANDED_FLING);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.SERP_NAVIGATION),
                EnterMaximizedFrom.EXPANDED_SERP_NAVIGATION);
        ENTER_MAXIMIZED_STATE_CHANGE_CODES = Collections.unmodifiableMap(codes);
    }

    // Exit code map: first exit from CLOSED.
    private static final Map<StateChangeKey, Integer> EXIT_CLOSED_TO_STATE_CHANGE_CODES;
    static {
        Map<StateChangeKey, Integer> codes = new HashMap<StateChangeKey, Integer>();
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.TEXT_SELECT_TAP),
                ExitClosedTo.PEEKED_TEXT_SELECT_TAP);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.TEXT_SELECT_LONG_PRESS),
                ExitClosedTo.PEEKED_TEXT_SELECT_LONG_PRESS);
        EXIT_CLOSED_TO_STATE_CHANGE_CODES = Collections.unmodifiableMap(codes);
    }

    // Exit code map: first exit from PEEKED.
    private static final Map<StateChangeKey, Integer> EXIT_PEEKED_TO_STATE_CHANGE_CODES;
    static {
        Map<StateChangeKey, Integer> codes = new HashMap<StateChangeKey, Integer>();
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.BACK_PRESS),
                ExitPeekedTo.CLOSED_BACK_PRESS);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.BASE_PAGE_SCROLL),
                ExitPeekedTo.CLOSED_BASE_PAGE_SCROLL);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.BASE_PAGE_TAP),
                ExitPeekedTo.CLOSED_TEXT_SELECT_TAP);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.TEXT_SELECT_TAP),
                ExitPeekedTo.PEEKED_TEXT_SELECT_TAP);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.TEXT_SELECT_LONG_PRESS),
                ExitPeekedTo.PEEKED_TEXT_SELECT_LONG_PRESS);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.SEARCH_BAR_TAP),
                ExitPeekedTo.EXPANDED_SEARCH_BAR_TAP);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.SWIPE),
                ExitPeekedTo.EXPANDED_SWIPE);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.FLING),
                ExitPeekedTo.EXPANDED_FLING);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.SWIPE),
                ExitPeekedTo.MAXIMIZED_SWIPE);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.FLING),
                ExitPeekedTo.MAXIMIZED_FLING);
        EXIT_PEEKED_TO_STATE_CHANGE_CODES = Collections.unmodifiableMap(codes);
    }

    // Exit code map: first exit from EXPANDED.
    private static final Map<StateChangeKey, Integer> EXIT_EXPANDED_TO_STATE_CHANGE_CODES;
    static {
        Map<StateChangeKey, Integer> codes = new HashMap<StateChangeKey, Integer>();
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.BACK_PRESS),
                ExitExpandedTo.CLOSED_BACK_PRESS);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.BASE_PAGE_TAP),
                ExitExpandedTo.CLOSED_BASE_PAGE_TAP);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.FLING),
                ExitExpandedTo.CLOSED_FLING);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.SEARCH_BAR_TAP),
                ExitExpandedTo.PEEKED_SEARCH_BAR_TAP);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.SWIPE),
                ExitExpandedTo.PEEKED_SWIPE);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.FLING),
                ExitExpandedTo.PEEKED_FLING);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.SWIPE),
                ExitExpandedTo.MAXIMIZED_SWIPE);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.FLING),
                ExitExpandedTo.MAXIMIZED_FLING);
        codes.put(new StateChangeKey(PanelState.MAXIMIZED, StateChangeReason.SERP_NAVIGATION),
                ExitExpandedTo.MAXIMIZED_SERP_NAVIGATION);
        EXIT_EXPANDED_TO_STATE_CHANGE_CODES = Collections.unmodifiableMap(codes);
    }

    // Exit code map: first exit from MAXIMIZED.
    private static final Map<StateChangeKey, Integer> EXIT_MAXIMIZED_TO_STATE_CHANGE_CODES;
    static {
        Map<StateChangeKey, Integer> codes = new HashMap<StateChangeKey, Integer>();
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.BACK_PRESS),
                ExitMaximizedTo.CLOSED_BACK_PRESS);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.FLING),
                ExitMaximizedTo.CLOSED_FLING);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.TAB_PROMOTION),
                ExitMaximizedTo.CLOSED_TAB_PROMOTION);
        codes.put(new StateChangeKey(PanelState.CLOSED, StateChangeReason.SERP_NAVIGATION),
                ExitMaximizedTo.CLOSED_SERP_NAVIGATION);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.SWIPE),
                ExitMaximizedTo.PEEKED_SWIPE);
        codes.put(new StateChangeKey(PanelState.PEEKED, StateChangeReason.FLING),
                ExitMaximizedTo.PEEKED_FLING);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.SWIPE),
                ExitMaximizedTo.EXPANDED_SWIPE);
        codes.put(new StateChangeKey(PanelState.EXPANDED, StateChangeReason.FLING),
                ExitMaximizedTo.EXPANDED_FLING);
        EXIT_MAXIMIZED_TO_STATE_CHANGE_CODES = Collections.unmodifiableMap(codes);
    }

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

    // "Promo outcome by gesture" code map: logged on exit from promo, broken down by gesture.
    private static final Map<Pair<Integer, Boolean>, Integer> PROMO_BY_GESTURE_CODES;
    static {
        Map<Pair<Integer, Boolean>, Integer> codes =
                new HashMap<Pair<Integer, Boolean>, Integer>();
        codes.put(new Pair<Integer, Boolean>(Preference.ENABLED, TAP), Promo.ENABLED_FROM_TAP);
        codes.put(new Pair<Integer, Boolean>(Preference.DISABLED, TAP), Promo.DISABLED_FROM_TAP);
        codes.put(new Pair<Integer, Boolean>(Preference.UNINITIALIZED, TAP),
                Promo.UNDECIDED_FROM_TAP);
        codes.put(new Pair<Integer, Boolean>(Preference.ENABLED, LONG_PRESS),
                Promo.ENABLED_FROM_LONG_PRESS);
        codes.put(new Pair<Integer, Boolean>(Preference.DISABLED, LONG_PRESS),
                Promo.DISABLED_FROM_LONG_PRESS);
        codes.put(new Pair<Integer, Boolean>(Preference.UNINITIALIZED, LONG_PRESS),
                Promo.UNDECIDED_FROM_LONG_PRESS);
        PROMO_BY_GESTURE_CODES = Collections.unmodifiableMap(codes);
    }

    /**
     * Logs the state of the Contextual Search preference. This function should be called if the
     * Contextual Search feature is active, and will track the different preference settings
     * (disabled, enabled or uninitialized). Calling more than once is fine.
     */
    public static void logPreferenceState() {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchPreferenceState",
                getPreferenceValue(), Preference.NUM_ENTRIES);
    }

    /**
     * Logs the given number of promo taps remaining.  Should be called only for users that
     * are still undecided.
     * @param promoTapsRemaining The number of taps remaining (should not be negative).
     */
    public static void logPromoTapsRemaining(int promoTapsRemaining) {
        if (promoTapsRemaining >= 0) {
            RecordHistogram.recordCountHistogram("Search.ContextualSearchPromoTapsRemaining",
                    promoTapsRemaining);
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
        RecordHistogram.recordCountHistogram("Search.ContextualSearchPromoTapsForNeverOpened",
                promoTaps);
    }

    /**
     * Logs the historic number of times that a Tap gesture triggered the peeking promo before
     * the user ever opened the panel.  This should be called periodically for all users.
     * @param promoTaps The historic number of taps that have caused the peeking bar for the promo
     *        before the first open of the panel, for all users that have ever opened the panel.
     */
    public static void logPromoTapsBeforeFirstOpen(int promoTaps) {
        RecordHistogram.recordCountHistogram("Search.ContextualSearchPromoTapsBeforeFirstOpen",
                promoTaps);
    }

    /**
     * Records the total count of times the promo panel has *ever* been opened.  This should only
     * be called when the user is still undecided.
     * @param count The total historic count of times the panel has ever been opened for the
     *        current user.
     */
    public static void logPromoOpenCount(int count) {
        RecordHistogram.recordCountHistogram("Search.ContextualSearchPromoOpenCount", count);
    }

    /**
     * Logs the number of taps that have been counted since the user last opened the panel, for
     * undecided users.
     * @param tapsSinceOpen The number of taps to log.
     */
    public static void logTapsSinceOpenForUndecided(int tapsSinceOpen) {
        RecordHistogram.recordCountHistogram("Search.ContextualSearchTapsSinceOpenUndecided",
                tapsSinceOpen);
    }

    /**
     * Logs the number of taps that have been counted since the user last opened the panel, for
     * decided users.
     * @param tapsSinceOpen The number of taps to log.
     */
    public static void logTapsSinceOpenForDecided(int tapsSinceOpen) {
        RecordHistogram.recordCountHistogram("Search.ContextualSearchTapsSinceOpenDecided",
                tapsSinceOpen);
    }

    /**
     * Logs whether the Search Term was single or multiword.
     * @param isSingleWord Whether the resolved search term is a single word or not.
     */
    public static void logSearchTermResolvedWords(boolean isSingleWord) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchResolvedTermWords",
                isSingleWord ? ResolvedGranularity.SINGLE_WORD : ResolvedGranularity.MULTI_WORD,
                ResolvedGranularity.NUM_ENTRIES);
    }

    /**
     * Logs whether the base page was using the HTTP protocol or not.
     * @param isHttpBasePage Whether the base page was using the HTTP protocol or not (should
     *        be false for HTTPS or other URIs).
     */
    public static void logBasePageProtocol(boolean isHttpBasePage) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchBasePageProtocol",
                isHttpBasePage ? Protocol.IS_HTTP : Protocol.NOT_HTTP, Protocol.NUM_ENTRIES);
    }

    /**
     * Logs changes to the Contextual Search preference, aside from those resulting from the first
     * run flow.
     * @param enabled Whether the preference is being enabled or disabled.
     */
    public static void logPreferenceChange(boolean enabled) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchPreferenceStateChange",
                enabled ? Preference.ENABLED : Preference.DISABLED, Preference.NUM_ENTRIES);
    }

    /**
     * Logs the outcome of the Promo.
     * Logs multiple histograms; with and without the originating gesture.
     * @param wasTap Whether the gesture that originally caused the panel to show was a Tap.
     * @param wasMandatory Whether the Promo was mandatory.
     */
    public static void logPromoOutcome(boolean wasTap, boolean wasMandatory) {
        int preferenceCode = getPreferenceValue();
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchFirstRunFlowOutcome",
                preferenceCode, Preference.NUM_ENTRIES);

        int preferenceByGestureCode = getPromoByGestureStateCode(preferenceCode, wasTap);
        if (wasMandatory) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Search.ContextualSearchMandatoryPromoOutcomeByGesture",
                    preferenceByGestureCode, Promo.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Search.ContextualSearchPromoOutcomeByGesture", preferenceByGestureCode,
                    Promo.NUM_ENTRIES);
        }
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
     * When Contextual Search panel is opened, logs whether In-Product Help for opening the panel
     * was ever shown.
     * @param wasIPHShown Whether In-Product help was shown.
     */
    public static void logPanelOpenedIPH(boolean wasIPHShown) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchPanelOpenedIPHShown", wasIPHShown);
    }

    /**
     * When Contextual Search panel is opened, logs whether In-Product Help for Contextual Search
     * was ever shown.
     * @param wasIPHShown Whether In-Product help was shown.
     */
    public static void logContextualSearchIPH(boolean wasIPHShown) {
        RecordHistogram.recordBooleanHistogram("Search.ContextualSearchIPHShown", wasIPHShown);
    }

    /**
     * When Contextual Search is triggered by tapping, logs whether In-Product Help for tapping was
     * ever shown.
     * @param wasIPHShown Whether In-Product help was shown.
     */
    public static void logTapIPH(boolean wasIPHShown) {
        RecordHistogram.recordBooleanHistogram("Search.ContextualSearchTapIPHShown", wasIPHShown);
    }

    /**
     * Logs whether we have ever shown an In-Product Help for Translations suggesting that the user
     * Opt-in.
     * @param wasIPHShown Whether In-Product help was shown.
     */
    public static void logTranslationsOptInIPHShown(boolean wasIPHShown) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.TranslationsOptInIPHShown", wasIPHShown);
    }

    /**
     * Logs whether the user actually did opt-in after seeing the In-Product Help for Translations
     * suggesting that the user should Opt-in.
     * @param didOptIn Whether the user did opt-in.
     */
    public static void logTranslationsOptInIPHWorked(boolean didOptIn) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearch.TranslationsOptInIPHWorked", didOptIn);
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
        if (ProfileSyncService.get() != null && ProfileSyncService.get().isSyncRequested()) {
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
     * Logs the whether the panel was seen and the type of the trigger and if Bar nearly overlapped.
     * If the panel was seen, logs the duration of the panel view into a BarOverlap or BarNoOverlap
     * duration histogram.
     * @param wasPanelSeen Whether the panel was seen.
     * @param wasTap Whether the gesture was a Tap or not.
     * @param wasBarOverlap Whether the trigger location overlapped the Bar area.
     */
    public static void logBarOverlapResultsSeen(
            boolean wasPanelSeen, boolean wasTap, boolean wasBarOverlap) {
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchBarOverlapSeen",
                getBarOverlapEnum(wasBarOverlap, wasPanelSeen, wasTap),
                BarOverlapResults.NUM_ENTRIES);
    }

    /**
     * Logs the duration of the panel viewed in its Peeked state before being opened.
     * @param wasBarOverlap Whether the trigger location overlapped the Bar area.
     * @param panelPeekDurationMs The duration that the panel was peeking before being opened
     *        by the user.
     */
    public static void logBarOverlapPeekDuration(boolean wasBarOverlap, long panelPeekDurationMs) {
        String histogram = wasBarOverlap ? "Search.ContextualSearchBarOverlap.PeekDuration"
                                         : "Search.ContextualSearchBarNoOverlap.PeekDuration";
        RecordHistogram.recordMediumTimesHistogram(histogram, panelPeekDurationMs);
    }

    /**
     * Log whether the UX was suppressed due to Bar overlap.
     * @param wasSuppressed Whether showing the UX was suppressed.
     */
    public static void logBarOverlapSuppression(boolean wasSuppressed) {
        RecordHistogram.recordBooleanHistogram("Search.ContextualSearchBarOverlap", wasSuppressed);
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
     * Logs the location of a Tap and whether the panel was seen and the type of the
     * trigger.
     * @param wasPanelSeen Whether the panel was seen.
     * @param wasTap Whether the gesture was a Tap or not.
     * @param triggerLocationDps The trigger location from the top of the screen.
     */
    public static void logScreenTopTapLocation(
            boolean wasPanelSeen, boolean wasTap, int triggerLocationDps) {
        // We only log Tap locations for the screen top.
        if (!wasTap) return;
        String histogram = wasPanelSeen ? "Search.ContextualSearchTopLocationSeen"
                                        : "Search.ContextualSearchTopLocationNotSeen";
        int min = 1;
        int max = 250;
        int numBuckets = 50;
        RecordHistogram.recordCustomCountHistogram(
                histogram, triggerLocationDps, min, max, numBuckets);
    }

    /**
     * Log whether the UX was suppressed due to a Tap too close to the screen top.
     * @param wasSuppressed Whether showing the UX was suppressed.
     */
    public static void logScreenTopTapSuppression(boolean wasSuppressed) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchScreenTopSuppressed", wasSuppressed);
    }

    /**
     * Log whether results were seen due to a Tap that was allowed to override an ML suppression.
     * @param wasSearchContentViewSeen If the panel was opened.
     */
    static void logSecondTapMlOverrideResultsSeen(boolean wasSearchContentViewSeen) {
        RecordHistogram.recordBooleanHistogram(
                "Search.ContextualSearchSecondTapMlOverrideSeen", wasSearchContentViewSeen);
    }

    /**
     * Logs whether results were seen based on the duration of the Tap, for both short and long
     * durations.
     * @param wasSearchContentViewSeen If the panel was opened.
     * @param isTapShort Whether this tap was "short" in duration.
     */
    public static void logTapDurationSeen(boolean wasSearchContentViewSeen, boolean isTapShort) {
        if (isTapShort) {
            RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchTapShortDurationSeen",
                    wasSearchContentViewSeen ? Results.SEEN : Results.NOT_SEEN,
                    Results.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchTapLongDurationSeen",
                    wasSearchContentViewSeen ? Results.SEEN : Results.NOT_SEEN,
                    Results.NUM_ENTRIES);
        }
    }

    /**
     * Logs the duration of a Tap in ms into custom histograms to profile the duration of seen
     * and not seen taps.
     * @param wasPanelSeen Whether the panel was seen.
     * @param durationMs The duration of the tap gesture.
     */
    public static void logTapDuration(boolean wasPanelSeen, int durationMs) {
        int min = 1;
        int max = 1000;
        int numBuckets = 100;

        if (wasPanelSeen) {
            RecordHistogram.recordCustomCountHistogram(
                    "Search.ContextualSearchTapDurationSeen", durationMs, min, max, numBuckets);
        } else {
            RecordHistogram.recordCustomCountHistogram(
                    "Search.ContextualSearchTapDurationNotSeen", durationMs, min, max, numBuckets);
        }
    }

    /**
     * Log whether results were seen due to a Tap on a short word.
     * @param wasSearchContentViewSeen If the panel was opened.
     * @param isTapOnShortWord Whether this tap was on a "short" word.
     */
    public static void logTapShortWordSeen(
            boolean wasSearchContentViewSeen, boolean isTapOnShortWord) {
        if (!isTapOnShortWord) return;

        // We just record CTR of short words.
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchTapShortWordSeen",
                wasSearchContentViewSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
    }

    /**
     * Log whether results were seen due to a Tap on a long word.
     * @param wasSearchContentViewSeen If the panel was opened.
     * @param isTapOnLongWord Whether this tap was on a long word.
     */
    public static void logTapLongWordSeen(
            boolean wasSearchContentViewSeen, boolean isTapOnLongWord) {
        if (!isTapOnLongWord) return;

        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchTapLongWordSeen",
                wasSearchContentViewSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
    }

    /**
     * Log whether results were seen due to a Tap that was on the middle of a word.
     * @param wasSearchContentViewSeen If the panel was opened.
     * @param isTapOnWordMiddle Whether this tap was on the middle of a word.
     */
    public static void logTapOnWordMiddleSeen(
            boolean wasSearchContentViewSeen, boolean isTapOnWordMiddle) {
        if (!isTapOnWordMiddle) return;

        // We just record CTR of words tapped in the "middle".
        RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchTapOnWordMiddleSeen",
                wasSearchContentViewSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
    }

    /**
     * Log whether results were seen due to a Tap on what we've recognized as a probable entity.
     * @param wasSearchContentViewSeen If the panel was opened.
     * @param isWordAnEntity Whether this tap was on a word that's an entity.
     */
    public static void logTapOnEntitySeen(
            boolean wasSearchContentViewSeen, boolean isWordAnEntity) {
        if (isWordAnEntity) {
            // We just record CTR of probable entity words.
            RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchEntitySeen",
                    wasSearchContentViewSeen ? Results.SEEN : Results.NOT_SEEN,
                    Results.NUM_ENTRIES);
        }
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
     * Logs how a state was entered for the first time within a Contextual Search.
     * @param fromState The state to transition from.
     * @param toState The state to transition to.
     * @param reason The reason for the state transition.
     */
    public static void logFirstStateEntry(
            @PanelState int fromState, @PanelState int toState, @StateChangeReason int reason) {
        int code;
        switch (toState) {
            case PanelState.CLOSED:
                code = getStateChangeCode(
                        fromState, reason, ENTER_CLOSED_STATE_CHANGE_CODES, EnterClosedFrom.OTHER);
                RecordHistogram.recordEnumeratedHistogram(
                        "Search.ContextualSearchEnterClosed", code, EnterClosedFrom.NUM_ENTRIES);
                break;
            case PanelState.PEEKED:
                code = getStateChangeCode(
                        fromState, reason, ENTER_PEEKED_STATE_CHANGE_CODES, EnterPeekedFrom.OTHER);
                RecordHistogram.recordEnumeratedHistogram(
                        "Search.ContextualSearchEnterPeeked", code, EnterPeekedFrom.NUM_ENTRIES);
                break;
            case PanelState.EXPANDED:
                code = getStateChangeCode(fromState, reason, ENTER_EXPANDED_STATE_CHANGE_CODES,
                        EnterExpandedFrom.OTHER);
                RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchEnterExpanded",
                        code, EnterExpandedFrom.NUM_ENTRIES);
                break;
            case PanelState.MAXIMIZED:
                code = getStateChangeCode(fromState, reason, ENTER_MAXIMIZED_STATE_CHANGE_CODES,
                        EnterMaximizedFrom.OTHER);
                RecordHistogram.recordEnumeratedHistogram("Search.ContextualSearchEnterMaximized",
                        code, EnterMaximizedFrom.NUM_ENTRIES);
                break;
            default:
                break;
        }
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
     * Logs how a state was exited for the first time within a Contextual Search.
     * @param fromState The state to transition from.
     * @param toState The state to transition to.
     * @param reason The reason for the state transition.
     */
    public static void logFirstStateExit(
            @PanelState int fromState, @PanelState int toState, @StateChangeReason int reason) {
        int code;
        switch (fromState) {
            case PanelState.UNDEFINED:
            case PanelState.CLOSED:
                code = getStateChangeCode(
                        toState, reason, EXIT_CLOSED_TO_STATE_CHANGE_CODES, ExitClosedTo.OTHER);
                RecordHistogram.recordEnumeratedHistogram(
                        "Search.ContextualSearchExitClosed", code, ExitClosedTo.NUM_ENTRIES);
                break;
            case PanelState.PEEKED:
                code = getStateChangeCode(
                        toState, reason, EXIT_PEEKED_TO_STATE_CHANGE_CODES, ExitPeekedTo.OTHER);
                RecordHistogram.recordEnumeratedHistogram(
                        "Search.ContextualSearchExitPeeked", code, ExitPeekedTo.NUM_ENTRIES);
                break;
            case PanelState.EXPANDED:
                code = getStateChangeCode(
                        toState, reason, EXIT_EXPANDED_TO_STATE_CHANGE_CODES, ExitExpandedTo.OTHER);
                RecordHistogram.recordEnumeratedHistogram(
                        "Search.ContextualSearchExitExpanded", code, ExitExpandedTo.NUM_ENTRIES);
                break;
            case PanelState.MAXIMIZED:
                code = getStateChangeCode(toState, reason, EXIT_MAXIMIZED_TO_STATE_CHANGE_CODES,
                        ExitMaximizedTo.OTHER);
                RecordHistogram.recordEnumeratedHistogram(
                        "Search.ContextualSearchExitMaximized", code, ExitMaximizedTo.NUM_ENTRIES);
                break;
            default:
                break;
        }
    }

    /**
     * Logs the number of impressions and CTR for the previous week for the current user.
     * @param previousWeekImpressions The number of times the user saw the Contextual Search Bar.
     * @param previousWeekCtr The CTR expressed as a percentage.
     */
    public static void logPreviousWeekCtr(int previousWeekImpressions, int previousWeekCtr) {
        RecordHistogram.recordCountHistogram(
                "Search.ContextualSearchPreviousWeekImpressions", previousWeekImpressions);
        RecordHistogram.recordPercentageHistogram(
                "Search.ContextualSearchPreviousWeekCtr", previousWeekCtr);
    }

    /**
     * Logs the number of impressions and CTR for previous 28-day period for the current user.
     * @param previous28DayImpressions The number of times the user saw the Contextual Search Bar.
     * @param previous28DayCtr The CTR expressed as a percentage.
     */
    public static void logPrevious28DayCtr(int previous28DayImpressions, int previous28DayCtr) {
        RecordHistogram.recordCountHistogram(
                "Search.ContextualSearchPrevious28DayImpressions", previous28DayImpressions);
        RecordHistogram.recordPercentageHistogram(
                "Search.ContextualSearchPrevious28DayCtr", previous28DayCtr);
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
     * Get the encoded value to use for the Bar Overlap histogram by encoding all the input
     * parameters.
     * @param didBarOverlap Whether the selection overlapped the Bar position.
     * @param wasPanelSeen Whether the panel content was seen.
     * @param wasTap Whether the gesture was a Tap.
     * @return The value for the enum histogram.
     */
    private static @BarOverlapResults int getBarOverlapEnum(
            boolean didBarOverlap, boolean wasPanelSeen, boolean wasTap) {
        if (wasTap) {
            if (didBarOverlap) {
                return wasPanelSeen ? BarOverlapResults.BAR_OVERLAP_RESULTS_SEEN_FROM_TAP
                                    : BarOverlapResults.BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_TAP;
            } else {
                return wasPanelSeen ? BarOverlapResults.NO_BAR_OVERLAP_RESULTS_SEEN_FROM_TAP
                                    : BarOverlapResults.NO_BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_TAP;
            }
        } else {
            if (didBarOverlap) {
                return wasPanelSeen
                        ? BarOverlapResults.BAR_OVERLAP_RESULTS_SEEN_FROM_LONG_PRESS
                        : BarOverlapResults.BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_LONG_PRESS;
            } else {
                return wasPanelSeen
                        ? BarOverlapResults.NO_BAR_OVERLAP_RESULTS_SEEN_FROM_LONG_PRESS
                        : BarOverlapResults.NO_BAR_OVERLAP_RESULTS_NOT_SEEN_FROM_LONG_PRESS;
            }
        }
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
     * Logs whether results were seen when Contextual Cards data was shown.
     * @param wasSeen Whether the search results were seen.
     */
    public static void logContextualCardsResultsSeen(boolean wasSeen) {
        RecordHistogram.recordEnumeratedHistogram(
                "Search.ContextualSearchContextualCardsIntegration.ResultsSeen",
                wasSeen ? Results.SEEN : Results.NOT_SEEN, Results.NUM_ENTRIES);
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
     * Logs a histogram indicating which privacy permissions are available that Related Searches
     * cares about. This ignores any language constraint.
     * <p>This can be called multiple times for each user from any part of the code that's freqently
     * executed.
     * @param canSendUrl Whether this user has allowed sending page URL info to Google.
     * @param canSendContent Whether the user can send page content to Google (has accepted the
     *        Contextual Search opt-in).
     */
    static void logRelatedSearchesPermissionsForAllUsers(
            boolean canSendUrl, boolean canSendContent) {
        @Permissions
        int permissionsEnum;
        if (canSendUrl) {
            permissionsEnum =
                    canSendContent ? Permissions.SEND_URL_AND_CONTENT : Permissions.SEND_URL;
        } else {
            permissionsEnum = canSendContent ? Permissions.SEND_CONTENT : Permissions.SEND_NOTHING;
        }
        RecordHistogram.recordEnumeratedHistogram("Search.RelatedSearches.AllUserPermissions",
                permissionsEnum, Permissions.NUM_ENTRIES);
    }

    /**
     * Logs a histogram indicating that a user is qualified for the Related Searches experiment
     * regardless of whether that feature is enabled. This uses a boolean histogram but always
     * logs true in order to get a raw bucket count (without using a user action, as suggested
     * in the User Action Guidelines doc).
     * <p>We use this to gauge whether each group has a balanced number of qualified users.
     * Can be logged multiple times since we'll just look at the user-count of this histogram.
     * This should be called any time a gesture is detected that could trigger a Related Search
     * if the feature were enabled.
     */
    static void logRelatedSearchesQualifiedUsers() {
        RecordHistogram.recordBooleanHistogram("Search.RelatedSearches.QualifiedUsers", true);
    }

    /**
     * Gets the state-change code for the given parameters by doing a lookup in the given map.
     * @param state The panel state.
     * @param reason The reason the state changed.
     * @param stateChangeCodes The map of state and reason to code.
     * @param defaultCode The code to return if the given values are not found in the map.
     * @return The code to write into an enum histogram, based on the given map.
     */
    private static int getStateChangeCode(@PanelState int state, @StateChangeReason int reason,
            Map<StateChangeKey, Integer> stateChangeCodes, int defaultCode) {
        Integer code = stateChangeCodes.get(new StateChangeKey(state, reason));
        return code != null ? code : defaultCode;
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
     * Gets the promo-outcome code for the given parameter by doing a lookup in the
     * promo-by-gesture map.
     * @param preferenceValue The code for the current preference value.
     * @param wasTap Whether the gesture that originally caused the panel to show was a Tap.
     * @return The code to write into a promo-outcome histogram.
     */
    private static int getPromoByGestureStateCode(int preferenceValue, boolean wasTap) {
        return PROMO_BY_GESTURE_CODES.get(new Pair<Integer, Boolean>(preferenceValue, wasTap));
    }

    /**
     * @return The code for the Contextual Search preference.
     */
    private static int getPreferenceValue() {
        if (ContextualSearchManager.isContextualSearchUninitialized()) {
            return Preference.UNINITIALIZED;
        } else if (ContextualSearchManager.isContextualSearchDisabled()) {
            return Preference.DISABLED;
        }
        return Preference.ENABLED;
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
