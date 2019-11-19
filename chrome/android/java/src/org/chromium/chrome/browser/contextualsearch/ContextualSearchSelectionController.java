// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSetting;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Controls selection gesture interaction for Contextual Search.
 */
public class ContextualSearchSelectionController {
    /**
     * The type of selection made by the user.
     */
    @IntDef({SelectionType.UNDETERMINED, SelectionType.TAP, SelectionType.LONG_PRESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SelectionType {
        int UNDETERMINED = 0;
        int TAP = 1;
        int LONG_PRESS = 2;
        int RESOLVING_LONG_PRESS = 3;
    }

    private static final String TAG = "ContextualSearch";
    private static final String CONTAINS_WORD_PATTERN = "(\\w|\\p{L}|\\p{N})+";
    // A URL is:
    //   1:    scheme://
    //   1+:   any word char, _ or -
    //   1+:   . followed by 1+ of any word char, _ or -
    //   0-1:  0+ of any word char or .,@?^=%&:/~#- followed by any word char or @?^-%&/~+#-
    // TODO(twellington): expand accepted schemes?
    private static final Pattern URL_PATTERN = Pattern.compile("((http|https|file|ftp|ssh)://)"
            + "([\\w_-]+(?:(?:\\.[\\w_-]+)+))([\\w.,@?^=%&:/~+#-]*[\\w@?^=%&/~+#-])?");

    // Max selection length must be limited or the entire request URL can go past the 2K limit.
    private static final int MAX_SELECTION_LENGTH = 100;

    private static final int INVALID_DURATION = -1;
    // A default tap duration value when we can't compute it.
    private static final int DEFAULT_DURATION = 0;

    private final ChromeActivity mActivity;
    private final ContextualSearchSelectionHandler mHandler;
    private final float mPxToDp;
    private final Pattern mContainsWordPattern;

    private String mSelectedText;
    private @SelectionType int mSelectionType;
    private boolean mWasTapGestureDetected;
    // Reflects whether the last tap was valid and whether we still have a tap-based selection.
    private ContextualSearchTapState mLastTapState;
    // Whether the selection was automatically expanded due to an adjustment (e.g. Resolve).
    private boolean mDidExpandSelection;

    // Position of the selection.
    private float mX;
    private float mY;

    // Additional tap info from Mojo.
    int mFontSizeDips;
    int mTextRunLength;

    // The time of the most last scroll activity, or 0 if none.
    private long mLastScrollTimeNs;

    // When the last tap gesture happened.
    private long mTapTimeNanoseconds;

    // Whether the selection was empty before the most recent tap gesture.
    private boolean mWasSelectionEmptyBeforeTap;

    // The duration of the last tap gesture in milliseconds, or 0 if not set.
    private int mTapDurationMs = INVALID_DURATION;

    /** Tracks whether we're currently clearing the selection to prevent recursion. */
    private boolean mClearingSelection;

    /**
     * Whether the current selection has been adjusted or not.  If the user has adjusted the
     * selection we must request a resolve for this exact term rather than anything that overlaps,
     * and not expand the selection (since it was explicitly set by the user).
     */
    private boolean mIsAdjustedSelection;

    /** Whether the selection handles are currently showing. */
    private boolean mAreSelectionHandlesShown;

    private class ContextualSearchGestureStateListener implements GestureStateListener {
        @Override
        public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
            mHandler.handleScrollStart();
        }

        @Override
        public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
            mLastScrollTimeNs = System.nanoTime();
            mHandler.handleScrollEnd();
        }

        @Override
        public void onScrollUpdateGestureConsumed() {
            // The onScrollEnded notification is unreliable, so mark time during scroll updates too.
            // See crbug.com/600863.
            mLastScrollTimeNs = System.nanoTime();
        }

        @Override
        public void onTouchDown() {
            mTapTimeNanoseconds = System.nanoTime();
            mWasSelectionEmptyBeforeTap = TextUtils.isEmpty(mSelectedText);
        }
    }

    /**
     * Constructs a new Selection controller for the given activity.  Callbacks will be issued
     * through the given selection handler.
     * @param activity The {@link ChromeActivity} to control.
     * @param handler The handler for callbacks.
     */
    public ContextualSearchSelectionController(ChromeActivity activity,
            ContextualSearchSelectionHandler handler) {
        mActivity = activity;
        mHandler = handler;
        mPxToDp = 1.f / mActivity.getResources().getDisplayMetrics().density;
        mContainsWordPattern = Pattern.compile(CONTAINS_WORD_PATTERN);
        // TODO(donnd): remove when behind-the-flag bug fixed (crbug.com/786589).
        Log.i(TAG, "Tap suppression enabled: %s",
                ContextualSearchFieldTrial.getSwitch(
                        ContextualSearchSwitch.IS_CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION_ENABLED));
    }

    /**
     * Notifies that the base page has started loading a page.
     */
    void onBasePageLoadStarted() {
        resetAllStates();
    }

    /**
     * Notifies that a Context Menu has been shown.
     */
    void onContextMenuShown() {
        // Hide the UX.
        mHandler.handleSelectionDismissal();
    }

    /**
     * Notifies that the Contextual Search has ended.
     * @param reason The reason for ending the Contextual Search.
     */
    void onSearchEnded(@OverlayPanel.StateChangeReason int reason) {
        // Long press selections should remain visible after ending a Contextual Search.
        if (mSelectionType == SelectionType.TAP) clearSelection();
    }

    /**
     * Returns a new {@code GestureStateListener} that will listen for events in the Base Page.
     * This listener will handle all Contextual Search-related interactions that go through the
     * listener.
     */
    public ContextualSearchGestureStateListener getGestureStateListener() {
        return new ContextualSearchGestureStateListener();
    }

    /**
     * @return the {@link ChromeActivity}.
     */
    ChromeActivity getActivity() {
        // TODO(donnd): don't expose the activity.
        return mActivity;
    }

    /**
     * @return the type of the selection.
     */
    @SelectionType
    int getSelectionType() {
        return mSelectionType;
    }

    /**
     * @return the selected text.
     */
    String getSelectedText() {
        return mSelectedText;
    }

    /**
     * Overrides the current internal setting that tracks the selection.
     *
     * @param selection The new selection value.
     */
    void setSelectedText(String selection) {
        mSelectedText = selection;
    }

    /**
     * @return The Pixel to Device independent Pixel ratio.
     */
    float getPxToDp() {
        return mPxToDp;
    }

    /**
     * @return The time of the most recent scroll, or 0 if none.
     */
    long getLastScrollTime() {
        return mLastScrollTimeNs;
    }

    /**
     * Returns whether the current selection has been adjusted or not.
     * If it has been adjusted we must request a resolve for this exact term rather than anything
     * that overlaps as is the behavior with normal expanding resolves.
     * @return Whether an exact word match is required in the resolve.
     */
    boolean isAdjustedSelection() {
        return mIsAdjustedSelection;
    }

    /**
     * Clears the selection.
     */
    void clearSelection() {
        if (mClearingSelection) return;

        mClearingSelection = true;
        SelectionPopupController controller = getSelectionPopupController();
        if (controller != null) controller.clearSelection();
        resetSelectionStates();
        mClearingSelection = false;
    }

    /**
     * @return The {@link SelectionPopupController} for the base WebContents.
     */
    protected SelectionPopupController getSelectionPopupController() {
        WebContents baseContents = getBaseWebContents();
        return baseContents != null ? SelectionPopupController.fromWebContents(baseContents) : null;
    }

    /**
     * Handles a change in the current Selection.
     * @param selection The selection portion of the context.
     */
    void handleSelectionChanged(String selection) {
        if (mDidExpandSelection) {
            mSelectedText = selection;
            mDidExpandSelection = false;
            return;
        }

        if (TextUtils.isEmpty(selection) && !TextUtils.isEmpty(mSelectedText)) {
            mSelectedText = selection;
            mHandler.handleSelectionCleared();
            // When the user taps on the page it will place the caret in that position, which
            // will trigger a onSelectionChanged event with an empty string.
            if (mSelectionType == SelectionType.TAP) {
                // Since we mostly ignore a selection that's empty, we only need to partially reset.
                resetSelectionStates();
                return;
            }
        }

        mSelectedText = selection;

        if (mWasTapGestureDetected) {
            assert mSelectionType == SelectionType.TAP;
            handleSelection(selection, mSelectionType);
            mWasTapGestureDetected = false;
        } else {
            boolean isValidSelection = validateSelectionSuppression(selection);
            mHandler.handleSelectionModification(selection, isValidSelection, mX, mY);
        }
    }

    /**
     * Handles a notification that a selection event took place.
     * @param eventType The type of event that took place.
     * @param posXPix The x coordinate of the selection start handle.
     * @param posYPix The y coordinate of the selection start handle.
     */
    void handleSelectionEvent(@SelectionEventType int eventType, float posXPix, float posYPix) {
        boolean shouldHandleSelection = false;
        switch (eventType) {
            case SelectionEventType.SELECTION_HANDLES_SHOWN:
                mAreSelectionHandlesShown = true;
                mWasTapGestureDetected = false;
                mSelectionType = ChromeFeatureList.isEnabled(
                                         ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE)
                        ? SelectionType.RESOLVING_LONG_PRESS
                        : SelectionType.LONG_PRESS;
                shouldHandleSelection = true;
                SelectionPopupController controller = getSelectionPopupController();
                if (controller != null) mSelectedText = controller.getSelectedText();
                mIsAdjustedSelection = false;
                break;
            case SelectionEventType.SELECTION_HANDLES_CLEARED:
                // Selection handles have been hidden, but there may still be a selection.
                mAreSelectionHandlesShown = false;
                mHandler.handleSelectionDismissal();
                resetAllStates();
                break;
            case SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED:
                shouldHandleSelection = true;
                mIsAdjustedSelection = true;
                break;
            default:
        }

        mX = posXPix;
        mY = posYPix;
        if (shouldHandleSelection) {
            if (mSelectedText != null) {
                handleSelection(mSelectedText, mSelectionType);
            }
        }
    }

    /**
     * Re-enables selection modification handling and invokes
     * ContextualSearchSelectionHandler.handleSelection().
     * @param selection The text that was selected.
     * @param type The type of selection made by the user.
     */
    private void handleSelection(String selection, @SelectionType int type) {
        boolean isValidSelection = validateSelectionSuppression(selection);
        mHandler.handleSelection(selection, isValidSelection, type, mX, mY);
    }

    /**
     * Resets all internal state of this class, including the tap state.
     */
    private void resetAllStates() {
        resetSelectionStates();
        mLastTapState = null;
        mLastScrollTimeNs = 0;
        mTapTimeNanoseconds = 0;
        mTapDurationMs = INVALID_DURATION;
        mDidExpandSelection = false;
        mFontSizeDips = 0;
        mTextRunLength = 0;
    }

    /**
     * Resets all of the internal state of this class that handles the selection.
     */
    private void resetSelectionStates() {
        mSelectionType = SelectionType.UNDETERMINED;
        mSelectedText = null;

        mWasTapGestureDetected = false;
        mIsAdjustedSelection = false;
        mAreSelectionHandlesShown = false;
    }

    /**
     * Should be called when a new Tab is selected.
     * Resets all of the internal state of this class.
     */
    void onTabSelected() {
        resetAllStates();
    }

    /**
     * Handles an unhandled tap gesture.
     * @param x The x coordinate in px.
     * @param y The y coordinate in px.
     * @param fontSizeDips The font size in DPs.
     * @param textRunLength The run-length of the text of the tapped element.
     */
    void handleShowUnhandledTapUIIfNeeded(int x, int y, int fontSizeDips, int textRunLength) {
        mWasTapGestureDetected = false;
        // TODO(donnd): refactor to avoid needing a new handler API method as suggested by Pedro.
        if (mSelectionType != SelectionType.LONG_PRESS && !mAreSelectionHandlesShown) {
            if (mTapTimeNanoseconds != 0) {
                mTapDurationMs = (int) ((System.nanoTime() - mTapTimeNanoseconds)
                        / TimeUtils.NANOSECONDS_PER_MILLISECOND);
            }
            mWasTapGestureDetected = true;
            mSelectionType = SelectionType.TAP;
            mX = x;
            mY = y;
            mFontSizeDips = fontSizeDips;
            mTextRunLength = textRunLength;
            mHandler.handleValidTap();
        } else {
            // Long press, or long-press selection handles shown; reset last tap state.
            mLastTapState = null;
            mHandler.handleInvalidTap();
        }
    }

    /**
     * Handles Tap suppression by making a callback to either the handler's #handleSuppressedTap()
     * or #handleNonSuppressedTap() after a possible delay.
     * This should be called when the context is fully built (by gathering surrounding text
     * if needed, etc) but before showing any UX.
     * @param contextualSearchContext The {@link ContextualSearchContext} for the Tap gesture.
     * @param interactionRecorder The {@link ContextualSearchInteractionRecorder} currently being
     * used to measure or suppress the UI by Ranker.
     */
    void handleShouldSuppressTap(ContextualSearchContext contextualSearchContext,
            ContextualSearchInteractionRecorder interactionRecorder) {
        int x = (int) mX;
        int y = (int) mY;

        // TODO(donnd): Remove tap counters.
        if (mTapDurationMs == INVALID_DURATION) mTapDurationMs = DEFAULT_DURATION;
        TapSuppressionHeuristics tapHeuristics =
                new TapSuppressionHeuristics(this, mLastTapState, x, y, contextualSearchContext,
                        mTapDurationMs, mWasSelectionEmptyBeforeTap, mFontSizeDips, mTextRunLength);
        // TODO(donnd): Move to be called when the panel closes to work with states that change.
        tapHeuristics.logConditionState();

        // Tell the manager what it needs in order to log metrics on whether the tap would have
        // been suppressed if each of the heuristics were satisfied.
        mHandler.handleMetricsForWouldSuppressTap(tapHeuristics);

        boolean shouldSuppressTapBasedOnHeuristics = tapHeuristics.shouldSuppressTap();
        boolean shouldOverrideMlTapSuppression = tapHeuristics.shouldOverrideMlTapSuppression();

        // Make sure Tap Suppression features are consistent.
        assert !ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION_ENABLED)
                || interactionRecorder.isQueryEnabled()
            : "Tap Suppression requires the Ranker Query feature to be enabled!";

        // If we're suppressing based on heuristics then Ranker doesn't need to know about it.
        @AssistRankerPrediction
        int tapPrediction = AssistRankerPrediction.UNDETERMINED;
        if (!shouldSuppressTapBasedOnHeuristics) {
            tapHeuristics.logRankerTapSuppression(interactionRecorder);
            mHandler.logNonHeuristicFeatures(interactionRecorder);
            tapPrediction = interactionRecorder.runPredictionForTapSuppression();
            ContextualSearchUma.logRankerPrediction(tapPrediction);
        }

        // Make the suppression decision and act upon it.
        boolean shouldSuppressTapBasedOnRanker = (tapPrediction == AssistRankerPrediction.SUPPRESS)
                && ContextualSearchFieldTrial.getSwitch(
                        ContextualSearchSwitch.IS_CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION_ENABLED)
                && !shouldOverrideMlTapSuppression;
        if (shouldSuppressTapBasedOnHeuristics || shouldSuppressTapBasedOnRanker) {
            Log.i(TAG, "Tap suppressed due to Ranker: %s, heuristics: %s",
                    shouldSuppressTapBasedOnRanker, shouldSuppressTapBasedOnHeuristics);
            mHandler.handleSuppressedTap();
        } else {
            mHandler.handleNonSuppressedTap(mTapTimeNanoseconds);
        }

        if (mTapTimeNanoseconds != 0) {
            // Remember the tap state for subsequent tap evaluation.
            mLastTapState = new ContextualSearchTapState(
                    x, y, mTapTimeNanoseconds, shouldSuppressTapBasedOnRanker);
        } else {
            mLastTapState = null;
        }
    }

    /**
     * @return The Base Page's {@link WebContents}, or {@code null} if there is no current tab.
     */
    @Nullable
    WebContents getBaseWebContents() {
        Tab currentTab = mActivity.getActivityTab();
        if (currentTab == null) return null;

        return currentTab.getWebContents();
    }

    /**
     * Expands the current selection by the specified amounts.
     * @param selectionStartAdjust The start offset adjustment of the selection to use to highlight
     *                             the search term.
     * @param selectionEndAdjust The end offset adjustment of the selection to use to highlight
     *                           the search term.
     */
    void adjustSelection(int selectionStartAdjust, int selectionEndAdjust) {
        if (selectionStartAdjust == 0 && selectionEndAdjust == 0) return;
        WebContents basePageWebContents = getBaseWebContents();
        if (basePageWebContents != null) {
            mDidExpandSelection = true;
            basePageWebContents.adjustSelectionByCharacterOffset(
                    selectionStartAdjust, selectionEndAdjust, /* show_selection_menu = */ false);
        }
    }

    // ============================================================================================
    // Misc.
    // ============================================================================================

    /**
     * @return whether selection is empty, for testing.
     */
    @VisibleForTesting
    boolean isSelectionEmpty() {
        return TextUtils.isEmpty(mSelectedText);
    }

    /**
     * Evaluates whether the given selection is valid and notifies the handler about potential
     * selection suppression.
     * TODO(pedrosimonetti): substitute this once the system supports suppressing selections.
     * @param selection The given selection.
     * @return Whether the selection is valid.
     */
    private boolean validateSelectionSuppression(String selection) {
        boolean isValid = isValidSelection(selection);

        if (mSelectionType == SelectionType.TAP) {
            int minSelectionLength = ContextualSearchFieldTrial.getValue(
                    ContextualSearchSetting.MINIMUM_SELECTION_LENGTH);
            if (selection.length() < minSelectionLength) {
                isValid = false;
                ContextualSearchUma.logSelectionLengthSuppression(true);
            } else if (minSelectionLength > 0) {
                ContextualSearchUma.logSelectionLengthSuppression(false);
            }
        }

        return isValid;
    }

    /** Determines if the given selection is valid or not.
     * @param selection The selection portion of the context.
     * @return whether the given selection is considered a valid target for a search.
     */
    private boolean isValidSelection(String selection) {
        return isValidSelection(selection, getSelectionPopupController());
    }

    @VisibleForTesting
    boolean isValidSelection(String selection, SelectionPopupController controller) {
        if (selection.length() > MAX_SELECTION_LENGTH) return false;
        if (!doesContainAWord(selection)) return false;
        if (controller != null && controller.isFocusedNodeEditable()) return false;
        return true;
    }

    /**
     * Determines if the given selection contains a word or not.
     * @param selection The the selection to check for a word.
     * @return Whether the selection contains a word anywhere within it or not.
     */
    @VisibleForTesting
    public boolean doesContainAWord(String selection) {
        return mContainsWordPattern.matcher(selection).find();
    }

    /**
     * @param selectionContext The String including the surrounding text and the selection.
     * @param startOffset The offset to the start of the selection (inclusive).
     * @param endOffset The offset to the end of the selection (non-inclusive).
     * @return Whether the selection is part of URL. A valid URL is:
     *         0-1:  schema://
     *         1+:   any word char, _ or -
     *         1+:   . followed by 1+ of any word char, _ or -
     *         0-1:  0+ of any word char or .,@?^=%&:/~#- followed by any word char or @?^-%&/~+#-
     */
    public static boolean isSelectionPartOfUrl(String selectionContext, int startOffset,
            int endOffset) {
        Matcher matcher = URL_PATTERN.matcher(selectionContext);

        // Starts are inclusive and ends are non-inclusive for both GSAContext & matcher.
        while (matcher.find()) {
            if (startOffset >= matcher.start() && endOffset <= matcher.end()) {
                return true;
            }
        }

        return false;
    }
}
