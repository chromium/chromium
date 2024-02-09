// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
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
 * Receives low-level events and feeds them to the {@link ContextualSearchManager}
 * while tracking the selection state.
 */
public class ContextualSearchSelectionController {
    /** The type of selection made by the user. */
    @IntDef({
        SelectionType.UNDETERMINED,
        SelectionType.TAP,
        SelectionType.LONG_PRESS,
        SelectionType.RESOLVING_LONG_PRESS
    })
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
    private static final Pattern URL_PATTERN =
            Pattern.compile(
                    "((http|https|file|ftp|ssh)://)"
                            + "([\\w_-]+(?:(?:\\.[\\w_-]+)+))([\\w.,@?^=%&:/~+#-]*[\\w@?^=%&/~+#-])?");

    // Max selection length must be limited or the entire request URL can go past the 2K limit.
    private static final int MAX_SELECTION_LENGTH = 1000;

    private final Activity mActivity;
    private final ContextualSearchSelectionHandler mHandler;
    private final float mPxToDp;
    private final Pattern mContainsWordPattern;

    /** A means of accessing the currently active tab. */
    private final Supplier<Tab> mTabSupplier;

    /**
     * The current selected text, either from tap or longpress, or {@code null} when the selection
     * has been programatically cleared.
     */
    @Nullable private String mSelectedText;

    /**
     * Identifies what caused the selection (Tap or Longpress) whenever the selection is not null.
     */
    private @SelectionType int mSelectionType;

    /**
     * A running tracker for the most recent valid selection type. This starts UNDETERMINED but
     * remains valid from then on.
     */
    private @SelectionType int mLastValidSelectionType;

    private boolean mWasTapGestureDetected;
    // Reflects whether the last tap was valid and whether we still have a tap-based selection.
    private ContextualSearchTapState mLastTapState;
    // Whether the selection was automatically expanded due to an adjustment (e.g. Resolve).
    private boolean mDidExpandSelection;

    // Position of the selection.
    private float mX;
    private float mY;

    // When the last tap gesture happened.
    private long mTapTimeNanoseconds;

    // Whether the selection was empty before the most recent tap gesture.
    private boolean mWasSelectionEmptyBeforeTap;

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

    /** Whether a drag of the selection handles is in progress. */
    private boolean mAreSelectionHandlesBeingDragged;

    private class ContextualSearchGestureStateListener extends GestureStateListener {
        @Override
        public void onScrollStarted(int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
            mHandler.handleScrollStart();
        }

        @Override
        public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
            mHandler.handleScrollEnd();
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
     * @param activity The activity for resource and view access.
     * @param handler The handler for callbacks.
     * @param tabSupplier Access to the currently active tab.
     */
    public ContextualSearchSelectionController(
            Activity activity,
            ContextualSearchSelectionHandler handler,
            Supplier<Tab> tabSupplier) {
        mActivity = activity;
        mHandler = handler;
        mTabSupplier = tabSupplier;
        mPxToDp = 1.f / mActivity.getResources().getDisplayMetrics().density;
        mContainsWordPattern = Pattern.compile(CONTAINS_WORD_PATTERN);
    }

    /** Notifies that the base page has started loading a page. */
    void onBasePageLoadStarted() {
        resetAllStates();
    }

    /** Notifies that a Context Menu has been shown. */
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

    /** @return A supplier of the currently active tab. */
    Supplier<Tab> getTabSupplier() {
        return mTabSupplier;
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

    /** @return whether the selection was established with a Tap gesture. */
    boolean isTapSelection() {
        return mSelectionType == SelectionType.TAP;
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
     * Returns whether the current selection has been adjusted or not.
     * If it has been adjusted we must request a resolve for this exact term rather than anything
     * that overlaps as is the behavior with normal expanding resolves.
     * @return Whether an exact word match is required in the resolve.
     */
    boolean isAdjustedSelection() {
        return mIsAdjustedSelection;
    }

    /** Clears the selection. */
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
            // If the user is dragging the handles just update the Bar, otherwise make a new search.
            if (mAreSelectionHandlesBeingDragged) {
                boolean isValidSelection = validateSelectionSuppression(selection);
                mHandler.handleSelectionModification(selection, isValidSelection, mX, mY);
            } else {
                // Smart Selection can cause a longpress selection change without the handles
                // being dragged. In that case do a full handling of the new selection.
                handleSelection(selection, mSelectionType);
            }
        }
        mLastValidSelectionType = mSelectionType;
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
                mAreSelectionHandlesBeingDragged = false;
                mWasTapGestureDetected = false;
                mSelectionType = SelectionType.RESOLVING_LONG_PRESS;
                shouldHandleSelection = true;
                SelectionPopupController controller = getSelectionPopupController();
                if (controller != null) mSelectedText = controller.getSelectedText();
                mIsAdjustedSelection = false;
                ContextualSearchUma.logSelectionEstablished();
                break;
            case SelectionEventType.SELECTION_HANDLES_CLEARED:
                // Selection handles have been hidden, but there may still be a selection.
                mAreSelectionHandlesShown = false;
                mAreSelectionHandlesBeingDragged = false;
                mHandler.handleSelectionDismissal();
                resetAllStates();
                break;
            case SelectionEventType.SELECTION_HANDLE_DRAG_STARTED:
                mAreSelectionHandlesBeingDragged = true;
                break;
            case SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED:
                mAreSelectionHandlesBeingDragged = false;
                shouldHandleSelection = true;
                mIsAdjustedSelection = true;
                ContextualSearchUma.logSelectionAdjusted(mSelectedText);
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

    /** Resets all internal state of this class, including the tap state. */
    private void resetAllStates() {
        resetSelectionStates();
        mLastTapState = null;
        mTapTimeNanoseconds = 0;
        mDidExpandSelection = false;
    }

    /** Resets all of the internal state of this class that handles the selection. */
    private void resetSelectionStates() {
        mSelectionType = SelectionType.UNDETERMINED;
        mSelectedText = null;

        mWasTapGestureDetected = false;
        mIsAdjustedSelection = false;
        mAreSelectionHandlesShown = false;
        mAreSelectionHandlesBeingDragged = false;
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
     */
    void handleShowUnhandledTapUIIfNeeded(int x, int y) {
        mWasTapGestureDetected = false;
        // TODO(donnd): refactor to avoid needing a new handler API method as suggested by Pedro.
        if (mSelectionType != SelectionType.LONG_PRESS
                && !mAreSelectionHandlesShown
                && mLastValidSelectionType != SelectionType.LONG_PRESS
                && mLastValidSelectionType != SelectionType.RESOLVING_LONG_PRESS) {
            mWasTapGestureDetected = true;
            mSelectionType = SelectionType.TAP;
            mX = x;
            mY = y;
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
     */
    void handleShouldSuppressTap() {
        int x = (int) mX;
        int y = (int) mY;

        TapSuppressionHeuristics tapHeuristics =
                new TapSuppressionHeuristics(
                        this, mLastTapState, x, y, mWasSelectionEmptyBeforeTap);
        // TODO(donnd): Move to be called when the panel closes to work with states that change.
        tapHeuristics.logConditionState();

        // Tell the manager what it needs in order to log metrics on whether the tap would have
        // been suppressed if each of the heuristics were satisfied.
        mHandler.handleMetricsForWouldSuppressTap(tapHeuristics);

        boolean shouldSuppressTapBasedOnHeuristics = tapHeuristics.shouldSuppressTap();

        // Make the suppression decision and act upon it.
        if (shouldSuppressTapBasedOnHeuristics) {
            Log.i(TAG, "Tap suppressed due to heuristics: %s", shouldSuppressTapBasedOnHeuristics);
            mHandler.handleSuppressedTap();
        } else {
            mHandler.handleNonSuppressedTap(mTapTimeNanoseconds);
        }

        if (mTapTimeNanoseconds != 0) {
            // Remember the tap state for subsequent tap evaluation.
            mLastTapState = new ContextualSearchTapState(x, y, mTapTimeNanoseconds);
        } else {
            mLastTapState = null;
        }
    }

    /**
     * @return The Base Page's {@link WebContents}, or {@code null} if there is no current tab.
     */
    @Nullable
    WebContents getBaseWebContents() {
        Tab currentTab = mTabSupplier.get();
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
                    selectionStartAdjust, selectionEndAdjust, /* showSelectionMenu= */ false);
            ContextualSearchUma.logSelectionExpanded(isTapSelection());
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
        return isValidSelection(selection, getSelectionPopupController());
    }

    /**
     * Determines if the given selection is text and some other conditions needed to trigger the
     * feature.
     * @param selection The selection string to evaluate.
     * @param controller The popup controller so we can look at the focused node.
     * @return If the selection is OK for this feature.
     */
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
    public static boolean isSelectionPartOfUrl(
            String selectionContext, int startOffset, int endOffset) {
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
