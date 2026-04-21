// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.MotionEvent;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.ResizingState;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetWebUiContainer.TouchHandler;
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

    void onSheetStateChanged(@SheetState int state) {
        mCurrentSheetState = state;
        if (state == SheetState.PEEK) {
            mModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, 1.0f);
        } else if (state == SheetState.FULL || state == SheetState.HALF) {
            mModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, 0.0f);
        }
    }

    /**
     * Updates the alpha for the cross-fade effect.
     *
     * @param offsetPx The current offset height in pixels for the sheet.
     */
    void updateCrossFadeAlpha(float offsetPx) {
        if (mPeekHeight == 0) {
            // No peek view, so set the alpha to 0.0f (expanded content visible).
            mModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, 0.0f);
            return;
        }

        float alpha;
        int crossFadeMaxHeight = getSheetCrossFadeMaxHeight();
        if (offsetPx <= mPeekHeight) {
            alpha = 1.0f;
        } else if (offsetPx >= crossFadeMaxHeight) {
            alpha = 0.0f;
        } else {
            // Linear interpolation between mPeekHeight and crossFadeMaxHeight.
            // Alpha goes from 1.0 to 0.0.
            alpha = 1.0f - (offsetPx - mPeekHeight) / (float) (crossFadeMaxHeight - mPeekHeight);
        }

        mModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, alpha);
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
    TouchHandler getWebUiTouchHandler() {
        return mTouchArbitrator;
    }

    boolean isMaximized() {
        return mCurrentSheetState == SheetState.FULL;
    }

    private boolean isShowing() {
        return mCurrentSheetState != SheetState.HIDDEN;
    }

    private int getSheetCrossFadeMaxHeight() {
        return mPeekHeight * 2;
    }

    /** Inner class that arbitrates between scrolling the content and dragging the bottom sheet. */
    private class TouchArbitrator implements TouchHandler {
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

    /** Sets whether the sheet is resizing. */
    public void onSheetResizingStatusChanged(boolean isResizing) {
        mModel.set(TabBottomSheetProperties.IS_RESIZING, isResizing);
    }

    /** Updates the state used for resizing the sheet. */
    public void setToFlexibleHeight() {
        mModel.set(
                TabBottomSheetProperties.RESIZING_STATE,
                new ResizingState(/* atFixedHeight= */ false, /* webUiContainerHeight= */ -1));
    }

    /**
     * Updates the state used for resizing the sheet.
     *
     * @param maxOffset The maximum offset height for the sheet.
     */
    public void setToFixedHeight(@Px int maxOffset) {
        mModel.set(
                TabBottomSheetProperties.RESIZING_STATE,
                new ResizingState(/* atFixedHeight= */ true, maxOffset));
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
