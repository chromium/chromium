// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.ResizingState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for tab bottom sheet */
@NullMarked
public class TabBottomSheetMediator extends GestureStateListener {
    private static final int MIN_SHEET_HEIGHT_DP = 240;

    private final Context mContext;
    private final PropertyModel mModel;
    private final TouchArbitrator mTouchArbitrator;
    private final float mFullheightRatio;
    private final float mKeyboardShowingHeightRatio;

    private @SheetState int mCurrentSheetState = SheetState.HIDDEN;
    private int mPeekHeight;

    public TabBottomSheetMediator(
            Context context,
            PropertyModel model,
            CoBrowseViews coBrowseViews,
            float fullheightRatio,
            float keyboardShowingHeightRatio) {
        mContext = context;
        mModel = model;
        mTouchArbitrator = new TouchArbitrator();
        mFullheightRatio = fullheightRatio;
        mKeyboardShowingHeightRatio = keyboardShowingHeightRatio;
    }

    void onSheetStateChanged(@SheetState int state, boolean hasPeekView) {
        mCurrentSheetState = state;
        if (!hasPeekView) return;
        if (state == SheetState.PEEK) {
            showPeekViewAndHideExpandedContent();
        } else {
            hidePeekViewAndShowExpandedContent();
        }
    }

    private void showPeekViewAndHideExpandedContent() {
        // TODO(crbug.com/494311176): Implement cross-fade animation for peek view.
        mModel.set(TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA, 1.0f);
        mModel.set(
                TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY, View.VISIBLE);
    }

    private void hidePeekViewAndShowExpandedContent() {
        // TODO(crbug.com/494311176): Implement cross-fade animation for peek view.
        mModel.set(TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_ALPHA, 0.0f);
        mModel.set(TabBottomSheetProperties.PEEK_VIEW_AND_EXPANDED_CONTENT_VISIBILITY, View.GONE);
    }

    /** Sets the peek state header height for touch arbitration. */
    void setPeekHeight(int peekHeight) {
        mPeekHeight = peekHeight;
    }

    boolean isSheetHeightSufficient(@Px int maxSheetOffset) {
        int maxSheetOffsetDp =
                DisplayUtil.pxToDp(DisplayAndroid.getNonMultiDisplay(mContext), maxSheetOffset);
        return maxSheetOffsetDp >= MIN_SHEET_HEIGHT_DP;
    }

    /** Returns the touch handler for the WebUI container. */
    TabBottomSheetWebUiContainer.TouchHandler getWebUiTouchHandler() {
        return mTouchArbitrator;
    }

    boolean isMaximized() {
        return mCurrentSheetState == SheetState.FULL;
    }

    private boolean isShowing() {
        return mCurrentSheetState != SheetState.HIDDEN;
    }

    /** Inner class that arbitrates between scrolling the content and dragging the bottom sheet. */
    private class TouchArbitrator implements TabBottomSheetWebUiContainer.TouchHandler {
        private boolean mInterceptForSheet;

        @Override
        public boolean handleTouchEvent(TabBottomSheetWebUiContainer v, MotionEvent e) {
            if (!isShowing()) {
                return false;
            }

            if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
                // Determine if the touch started in the "Gesture Zone".
                int minTouchTargetPx =
                        v.getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.min_touch_target_size);

                // Use max() to ensure it meets the minimum touch target size of 48dp.
                int gestureZoneHeight = Math.max(mPeekHeight, minTouchTargetPx);

                // If the touch starts in the gesture zone (measured from the top of the
                // container), intercept the gesture for the bottom sheet.
                mInterceptForSheet = (!isMaximized() || e.getY() <= gestureZoneHeight);
            }

            if (mInterceptForSheet) {
                v.getParent().requestDisallowInterceptTouchEvent(false);
                return false;
            }

            // Lock to content and manually deliver.
            v.getParent().requestDisallowInterceptTouchEvent(true);
            v.dispatchTouchEvent(e);
            return true;
        }
    }

    public void onSheetResizingStatusChanged(boolean isResizing) {
        mModel.set(TabBottomSheetProperties.IS_RESIZING, isResizing);
    }

    /**
     * Updates the state used for resizing the sheet.
     *
     * @param defaultHeightRatio The default height ratio for the sheet.
     * @param heightFraction The current height fraction for the sheet.
     * @param offsetHeight The current offset height for the sheet.
     * @param maxOffset The maximum offset height for the sheet.
     */
    public void updateResizingState(
            float defaultHeightRatio,
            float heightFraction,
            @Px int offsetHeight,
            @Px int maxOffset) {
        if (!ChromeFeatureList.sTabBottomSheetResizeWebview.getValue()) {
            // Use the maxOffset since the sheet content should always assume full height when the
            // feature is off.
            mModel.set(TabBottomSheetProperties.RESIZING_STATE, new ResizingState(maxOffset, 1.0f));
            return;
        }

        int webUiHeight;
        if (heightFraction <= defaultHeightRatio) {
            // If the sheet offset is less than the default height ratio, lock the WebUi height to
            // the default height ratio.
            float lockedWebUiHeight = defaultHeightRatio * maxOffset;
            webUiHeight = (int) lockedWebUiHeight;
        } else {
            webUiHeight = (int) offsetHeight;
        }

        mModel.set(
                TabBottomSheetProperties.RESIZING_STATE,
                new ResizingState(webUiHeight, heightFraction));
    }

    @SheetState
    int getSheetStateForTesting() {
        return mCurrentSheetState;
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    float getFullHeightRatioForTesting() {
        return mFullheightRatio;
    }

    float getKeyboardShowingHeightRatioForTesting() {
        return mKeyboardShowingHeightRatio;
    }
}
