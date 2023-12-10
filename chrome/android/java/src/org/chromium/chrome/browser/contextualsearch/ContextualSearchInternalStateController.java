// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Controls the internal state of the Contextual Search Manager.
 * <p>
 * This class keeps track of the current internal state of the {@code ContextualSearchManager} and
 * helps it to transition between states and return to the idle state when work has been
 * interrupted or complete.
 * <p>
 * Usage: Call {@link #reset(StateChangeReason)} to reset to the {@code IDLE} state, which hides
 * the UI.<br>
 * Call {@link #enter(InternalState)} to enter a start-state (when a user gesture is recognized).
 * When doing some work on a state, which may be done in an asynchronous manner:<ol>
 * <li>call {@link #notifyStartingWorkOn(InternalState)} to note that work is starting on that state
 * <li>call {@link #notifyFinishedWorkOn(InternalState)} when work is completed.
 * <li>If a handler of an async response needs to do additional work, such as updating the UI, it
 * should first call {@link #isStillWorkingOn(InternalState)} to check that work has not been
 * interrupted since the async operation was started.
 * </ol><p>
 * The {@link #notifyFinishedWorkOn(InternalState)} method will automatically start a transition to
 * the appropriate next state.
 * <p>
 * Policy decisions about state transitions should only be done in the private
 * {@link #transitionTo(InternalState)} method of this class (not within the
 * {@code ContextualSearchManager} itself).
 */
class ContextualSearchInternalStateController {
    private static final String TAG = "ContextualSearch";

    private final ContextualSearchPolicy mPolicy;
    private final ContextualSearchInternalStateHandler mStateHandler;

    // The type of selection that triggered this state-change sequence;
    private @SelectionType int mSelectionType;

    /**
     * The current internal state of the {@code ContextualSearchManager}.
     * States can be "start states" which can be passed to #enter(), or "transitional states" which
     * automatically transition to the appropriate next state when work is done on them, or
     * "resting states" which do not transition into any next state, or a combination of the
     * above.
     */
    @IntDef({
        InternalState.UNDEFINED,
        InternalState.IDLE,
        InternalState.LONG_PRESS_RECOGNIZED,
        InternalState.SHOWING_LITERAL_SEARCH,
        InternalState.SELECTION_CLEARED_RECOGNIZED,
        InternalState.WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS,
        InternalState.TAP_RECOGNIZED,
        InternalState.WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION,
        InternalState.TAP_GESTURE_COMMIT,
        InternalState.GATHERING_SURROUNDINGS,
        InternalState.DECIDING_SUPPRESSION,
        InternalState.START_SHOWING_TAP_UI,
        InternalState.SHOW_RESOLVING_UI,
        InternalState.RESOLVING,
        InternalState.SHOWING_TAP_SEARCH,
        InternalState.RESOLVING_LONG_PRESS_RECOGNIZED,
        InternalState.SHOWING_RESOLVED_LONG_PRESS_SEARCH,
        InternalState.SEARCH_COMPLETED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface InternalState {
        /**
         * This start state should only be used when the manager is not yet initialized or already
         * destroyed.
         */
        int UNDEFINED = 0;

        /** This start/resting state shows no UI (panel is closed). */
        int IDLE = 1;

        /** This starts a transition that leads to the SHOWING_LITERAL_SEARCH state. */
        int LONG_PRESS_RECOGNIZED = 2;

        /** State showing the panel in response to a literal non-resolving search. */
        int SHOWING_LITERAL_SEARCH = 3;

        /**
         * This is a start state when the selection is cleared typically due to a tap on the base
         * page. If the previous state wasn't IDLE then it could be a tap near a previous Tap.
         * Transitions to WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS to wait for a Tap and hide the Bar
         * if no tap ever happens.
         */
        int SELECTION_CLEARED_RECOGNIZED = 4;

        /**
         * Waits to see if the tap gesture was valid so we can just update the Bar instead of
         * hiding/showing it.
         */
        int WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS = 5;

        /** This starts a sequence of states needed to get to the SHOWING_TAP_SEARCH state. */
        int TAP_RECOGNIZED = 6;

        /**
         * Waits to see if the Tap was on a previous tap-selection, which will show the selection
         * manipulation pins and be subsumed by a LONG_PRESS_RECOGNIZED.  If that doesn't happen
         * within the waiting period we'll advance.
         */
        int WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION = 7;

        /**
         * The first state in the Tap-gesture processing pipeline where we know we're processing
         * a Tap-gesture that won't be converted into a long-press (from tap on tap-selection).  It
         * may later be suppressed or ignored due to being on an invalid character.
         */
        int TAP_GESTURE_COMMIT = 8;

        /** Gathers text surrounding the selection. */
        int GATHERING_SURROUNDINGS = 9;

        /** Decides if the gesture should trigger the UX or be suppressed. */
        int DECIDING_SUPPRESSION = 10;

        /** Start showing the Tap UI. Currently this means select the word that was tapped. */
        int START_SHOWING_TAP_UI = 11;

        /** Show the full Tap UI. Currently this means showing the Overlay Panel. */
        int SHOW_RESOLVING_UI = 12;

        /**
         * Resolving the Search Term using the surrounding text and additional context.
         * Currently this makes a server request, which could take a long time.
         */
        int RESOLVING = 13;

        /** State when showing the panel in response to a Tap gesture. */
        int SHOWING_TAP_SEARCH = 14;

        /**
         * This starts the resolving transition that leads to the
         * SHOWING_RESOLVED_LONG_PRESS_SEARCH.
         */
        int RESOLVING_LONG_PRESS_RECOGNIZED = 15;

        /** State when showing the panel in response to a longpress gesture that resolved. */
        int SHOWING_RESOLVED_LONG_PRESS_SEARCH = 16;

        /** The final resting state for all searches once they have completed. */
        int SEARCH_COMPLETED = 17;
    }

    // The current state of this instance.
    private @InternalState int mState = InternalState.UNDEFINED;

    // Whether work has started on the current state.
    private boolean mDidStartWork;

    // The previous state of this instance.
    private @InternalState int mPreviousState = InternalState.UNDEFINED;

    /**
     * Constructs an instance of this class, which has the same lifetime as the
     * {@code ContextualSearchManager} and the given parameters.
     */
    ContextualSearchInternalStateController(
            ContextualSearchPolicy policy, ContextualSearchInternalStateHandler stateHandler) {
        mPolicy = policy;
        mStateHandler = stateHandler;
    }

    // ============================================================================================
    // State-transition management.
    // This code is designed to solve several problems:
    // 1) Document the sequencing of handling a gesture in code.  Now there's a single method that
    //    determines the sequence that should be followed for Tap handling (our most complicated
    //    case.
    // 2) Document the initiation and subsequent notification/handling of operations.  Now the
    //    method that starts an operation and the notification handler are tied together by their
    //    references to the same state.  This allows a simple search to find the
    //    initiation and handler together (which is not always easy, e.g. SelectWordAroundCaret
    //    does not yet have an ACK so we infer that it's complete when the selection change -- or
    //    does not change after some short waiting period).
    // 3) Gracefully handle sequence interruptions.  When an asynchronous operation is in progress
    //    the user may start a new sequence or abort the current sequence.  Now the handler for an
    //    asynchronous operation can easily detect that it's no longer working on that operation
    //    and skip the normal completion of the operation.
    // ============================================================================================

    /**
     * Reset the current state to the IDLE state.
     * @param reason The reason for the reset.
     */
    void reset(@Nullable @StateChangeReason Integer reason) {
        mSelectionType = SelectionType.UNDETERMINED;
        transitionTo(InternalState.IDLE, reason);
    }

    /**
     * Enters the given starting state immediately.
     * Note: This will synchronously complete the given state and process all subsequent
     * non-asynchronous states before returning.  See https://crbug.com/1099383.
     * @param state The new starting {@link InternalState} we're now in.
     */
    void enter(@InternalState int state) {
        assert state == InternalState.UNDEFINED
                || state == InternalState.IDLE
                || state == InternalState.LONG_PRESS_RECOGNIZED
                || state == InternalState.RESOLVING_LONG_PRESS_RECOGNIZED
                || state == InternalState.TAP_RECOGNIZED
                || state == InternalState.SELECTION_CLEARED_RECOGNIZED;
        mPreviousState = mState;
        mState = state;
        switch (state) {
            case InternalState.LONG_PRESS_RECOGNIZED:
                mSelectionType = SelectionType.LONG_PRESS;
                break;
            case InternalState.RESOLVING_LONG_PRESS_RECOGNIZED:
                mSelectionType = SelectionType.RESOLVING_LONG_PRESS;
                break;
            case InternalState.TAP_RECOGNIZED:
                mSelectionType = SelectionType.TAP;
                break;
            default:
                mSelectionType = SelectionType.UNDETERMINED;
        }
        notifyStartingWorkOn(mState);
        notifyFinishedWorkOn(mState);
    }

    /**
     * Confirms that work is starting on the given state.
     * @param state The {@link InternalState} that we're now working on.
     */
    void notifyStartingWorkOn(@InternalState int state) {
        assert mState == state;
        mDidStartWork = true;
    }

    /**
     * @return Whether we're still working on the given state.
     */
    boolean isStillWorkingOn(@InternalState int state) {
        return mState == state;
    }

    /**
     * Confirms that work has been finished on the given state, and will process all subsequent
     * non-asynchronous states before returning.  See https://crbug.com/1099383.
     * This should be called by every operation that waits for some kind of completion when it
     * completes.  The operation's start must be flagged using {@link #notifyStartingWorkOn}.
     * @param state The {@link InternalState} that we've finished working on.
     */
    void notifyFinishedWorkOn(@InternalState int state) {
        finishWorkingOn(state);
    }

    /**
     * Notifies that the given state has been started and completed. Useful when no work is needed.
     */
    void notifyStartedAndFinished(@InternalState int state) {
        notifyStartingWorkOn(state);
        notifyFinishedWorkOn(state);
    }

    /**
     * @return The current internal state for testing purposes.
     */
    @VisibleForTesting
    protected @InternalState int getState() {
        return mState;
    }

    /**
     * Establishes the given state by calling code that starts work on that state.
     * @param state The new {@link InternalState} to establish.
     */
    private void transitionTo(@InternalState int state) {
        transitionTo(state, null);
    }

    /**
     * Establishes the given state by calling code that starts work on that state or simply
     * displays the appropriate UX for that state.
     * @param state The new {@link InternalState} to establish.
     * @param reason The reason we're starting this state, or {@code null} if not significant
     *        or known.  Only needed when we enter the IDLE state.
     */
    private void transitionTo(
            final @InternalState int state, final @Nullable @StateChangeReason Integer reason) {
        if (state == mState && !mPolicy.shouldRetryCurrentState(state)) return;
        Log.v(TAG, "State transition " + String.valueOf(mState) + " => " + String.valueOf(state));

        // This should be the only part of the code that changes the state (other than #enter)!
        mPreviousState = mState;
        mState = state;

        mDidStartWork = false;
        startWorkingOn(state, reason);
    }

    /**
     * Starts working on the given state by calling code that starts work on that state or simply
     * displays the appropriate UX for that state.
     * @param state The new {@link InternalState} to establish.
     * @param reason The reason we're starting this state, or {@code null} if not significant
     *        or known.  Only needed when we enter the IDLE state.
     */
    private void startWorkingOn(
            @InternalState int state, @Nullable @StateChangeReason Integer reason) {
        // All transitional states should be listed here, but not start states.
        switch (state) {
            case InternalState.IDLE:
                assert reason != null;
                mStateHandler.hideContextualSearchUi(reason);
                break;
            case InternalState.SHOWING_LITERAL_SEARCH:
                mStateHandler.showContextualSearchLiteralSearchUi();
                break;
            case InternalState.WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS:
                mStateHandler.waitForPossibleTapNearPrevious();
                break;
            case InternalState.WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION:
                mStateHandler.waitForPossibleTapOnTapSelection();
                break;
            case InternalState.TAP_GESTURE_COMMIT:
                mStateHandler.tapGestureCommit();
                break;
            case InternalState.GATHERING_SURROUNDINGS:
                mStateHandler.gatherSurroundingText();
                break;
            case InternalState.DECIDING_SUPPRESSION:
                mStateHandler.decideSuppression();
                break;
            case InternalState.START_SHOWING_TAP_UI:
                mStateHandler.startShowingTapUi();
                break;
            case InternalState.SHOW_RESOLVING_UI:
                mStateHandler.showContextualSearchResolvingUi();
                break;
            case InternalState.RESOLVING:
                mStateHandler.resolveSearchTerm();
                break;
            case InternalState.SHOWING_TAP_SEARCH:
                mStateHandler.showingTapSearch();
                break;
            case InternalState.SHOWING_RESOLVED_LONG_PRESS_SEARCH:
                mStateHandler.showingIntelligentLongpress();
                break;
            case InternalState.SEARCH_COMPLETED:
                mStateHandler.completeSearch();
                break;
            default:
                Log.w(TAG, "Warning: unexpected startWorkingOn " + String.valueOf(state));
                break;
        }
    }

    /**
     * Finishes working on the given state by making a transition to the next state if needed.
     * @param state The {@link InternalState} that we've finished working on.
     */
    private void finishWorkingOn(@InternalState int state) {
        // When an async task finishes work some action may have caused a reset and now we're
        // in a new sequence, so no need to finish work on the abandoned state.
        if (state != mState) return;

        // Should have called #nofifyStartingWorkOn this state already.
        assert mDidStartWork;

        if (mState == InternalState.IDLE || mState == InternalState.UNDEFINED) {
            Log.w(TAG, "Warning, the " + String.valueOf(state) + " state was aborted.");
            return;
        }

        switch (state) {
            case InternalState.LONG_PRESS_RECOGNIZED:
                transitionTo(InternalState.GATHERING_SURROUNDINGS);
                break;
            case InternalState.SHOWING_LITERAL_SEARCH:
                transitionTo(InternalState.SEARCH_COMPLETED);
                break;
            case InternalState.SELECTION_CLEARED_RECOGNIZED:
                if (mPreviousState != InternalState.UNDEFINED
                        && mPreviousState != InternalState.IDLE) {
                    transitionTo(InternalState.WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS);
                } else {
                    reset(StateChangeReason.BASE_PAGE_TAP);
                }
                break;
            case InternalState.WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS:
                // If a tap near the previous was detected we've started another sequence and won't
                // get here. So we know the wait completed without any other action so we need to
                // reset the UX.
                reset(StateChangeReason.BASE_PAGE_TAP);
                break;
            case InternalState.TAP_RECOGNIZED:
                transitionTo(
                        mPreviousState != InternalState.UNDEFINED
                                        && mPreviousState != InternalState.IDLE
                                ? InternalState.WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION
                                : InternalState.TAP_GESTURE_COMMIT);
                break;
            case InternalState.WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION:
                transitionTo(InternalState.TAP_GESTURE_COMMIT);
                break;
            case InternalState.TAP_GESTURE_COMMIT:
                transitionTo(InternalState.GATHERING_SURROUNDINGS);
                break;
            case InternalState.GATHERING_SURROUNDINGS:
                // We gather surroundings for both Tap and Long-press in order to notify icing.
                if (mSelectionType == SelectionType.LONG_PRESS) {
                    transitionTo(InternalState.SHOWING_LITERAL_SEARCH);
                } else if (mSelectionType == SelectionType.RESOLVING_LONG_PRESS) {
                    transitionTo(InternalState.SHOW_RESOLVING_UI);
                } else {
                    transitionTo(InternalState.DECIDING_SUPPRESSION);
                }
                break;
            case InternalState.DECIDING_SUPPRESSION:
                transitionTo(InternalState.START_SHOWING_TAP_UI);
                break;
            case InternalState.START_SHOWING_TAP_UI:
                transitionTo(InternalState.SHOW_RESOLVING_UI);
                break;
            case InternalState.SHOW_RESOLVING_UI:
                transitionTo(
                        mPolicy.shouldPreviousGestureResolve()
                                ? InternalState.RESOLVING
                                : InternalState.SHOWING_TAP_SEARCH);
                break;
            case InternalState.RESOLVING:
                transitionTo(
                        mSelectionType == SelectionType.TAP
                                ? InternalState.SHOWING_TAP_SEARCH
                                : InternalState.SHOWING_RESOLVED_LONG_PRESS_SEARCH);
                break;
            case InternalState.RESOLVING_LONG_PRESS_RECOGNIZED:
                transitionTo(InternalState.GATHERING_SURROUNDINGS);
                break;
            case InternalState.SHOWING_RESOLVED_LONG_PRESS_SEARCH:
                transitionTo(InternalState.SEARCH_COMPLETED);
                break;
            case InternalState.SHOWING_TAP_SEARCH:
                transitionTo(InternalState.SEARCH_COMPLETED);
                break;
            case InternalState.SEARCH_COMPLETED:
                // Resting state
                break;
            default:
                Log.e(TAG, "The state " + String.valueOf(state) + " is not transitional!");
                assert false;
        }
    }
}
