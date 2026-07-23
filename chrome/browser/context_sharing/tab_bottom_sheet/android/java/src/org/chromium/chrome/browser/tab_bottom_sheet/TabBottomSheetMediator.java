// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.MotionEvent;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetWebUiContainer.TouchHandler;
import org.chromium.chrome.browser.tab_bottom_sheet.WebViewResizingHelper.ResizeLock;
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

    private @SheetState int mCurrentSheetState = SheetState.HIDDEN;
    private int mPeekHeight;
    private @Nullable ResizeLock mResizeLock;
    private boolean mLastIsBetweenDefaultAndFullHeight;
    private boolean mIsResizing;

    public TabBottomSheetMediator(Context context, PropertyModel model) {
        mContext = context;
        mModel = model;

        mTouchArbitrator = new TouchArbitrator();
    }

    /**
     * Updates the offset height fraction for the sheet during scrolling/resizing.
     *
     * @param offsetPx The offset in pixels.
     * @param isBetweenDefaultAndFullHeight Whether the current offset is between default and full
     *     height.
     */
    public void onSheetOffsetChanged(float offsetPx, boolean isBetweenDefaultAndFullHeight) {
        updateCrossFadeAlpha(offsetPx);
        if (mIsResizing) {
            updateResizingState(isBetweenDefaultAndFullHeight);
        }
    }

    /** Sets whether the sheet is resizing. */
    public void onSheetResizingStatusChanged(boolean isResizing) {
        mIsResizing = isResizing;
        if (!isResizing) {
            if (mResizeLock != null) {
                mResizeLock.unlock();
                mResizeLock = null;
            }
            return;
        }

        updateResizingState(mLastIsBetweenDefaultAndFullHeight);
    }

    private void updateResizingState(boolean isBetweenDefaultAndFullHeight) {
        WebViewResizingHelper helper =
                mModel.get(TabBottomSheetProperties.WEB_VIEW_RESIZING_HELPER);
        if (helper == null || !mIsResizing) {
            return;
        }

        if (isBetweenDefaultAndFullHeight) {
            if (mResizeLock == null) {
                mResizeLock = helper.requestResize();
            }
        } else if (mResizeLock != null) {
            // Suppress requestResize() when at or below default height (between peek and
            // default height).
            mResizeLock.unlock();
            mResizeLock = null;
        }

        mLastIsBetweenDefaultAndFullHeight = isBetweenDefaultAndFullHeight;
    }

    /**
     * Updates the height of the resizing placeholder inside the sheet.
     *
     * @param visibleHeight The visible height in pixels.
     */
    public void updatePlaceholderHeight(float visibleHeight) {
        WebViewResizingHelper helper =
                mModel.get(TabBottomSheetProperties.WEB_VIEW_RESIZING_HELPER);
        if (helper != null && mResizeLock != null) {
            helper.updatePlaceholderHeight((int) visibleHeight);
        }
    }

    /** Updates the state used for resizing the sheet. */
    public void setToFlexibleHeight() {
        WebViewResizingHelper helper =
                mModel.get(TabBottomSheetProperties.WEB_VIEW_RESIZING_HELPER);
        if (helper != null) {
            helper.setToFlexibleHeight();
        }
    }

    /**
     * Updates the state used for resizing the sheet.
     *
     * @param maxOffset The maximum offset height for the sheet.
     */
    public void setToFixedHeight(@Px int maxOffset) {
        WebViewResizingHelper helper =
                mModel.get(TabBottomSheetProperties.WEB_VIEW_RESIZING_HELPER);
        if (helper != null) {
            helper.setToFixedHeight(maxOffset);
        }
    }

    void onSheetStateChanged(@SheetState int state) {
        mCurrentSheetState = state;
        if (state == SheetState.PEEK) {
            mModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, 1.0f);
            mModel.set(TabBottomSheetProperties.EXPANDED_STATE_ALPHA, 0.0f);
        } else if (state == SheetState.FULL || state == SheetState.HALF) {
            mModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, 0.0f);
            mModel.set(TabBottomSheetProperties.EXPANDED_STATE_ALPHA, 1.0f);
        } else if (state == SheetState.HIDDEN) {
            mLastIsBetweenDefaultAndFullHeight = false;
        }
    }

    /**
     * Updates the alpha for the cross-fade effect.
     *
     * @param offsetPx The current offset height in pixels for the sheet.
     */
    void updateCrossFadeAlpha(float offsetPx) {
        if (mPeekHeight == 0) {
            mModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, 0.0f);
            mModel.set(TabBottomSheetProperties.EXPANDED_STATE_ALPHA, 1.0f);
            return;
        }

        float peekAlpha;
        float expandedAlpha;
        int crossFadeMaxHeight = getSheetCrossFadeMaxHeight();
        float midpoint = mPeekHeight + (crossFadeMaxHeight - mPeekHeight) / 2.0f;

        if (offsetPx <= mPeekHeight) {
            peekAlpha = 1.0f;
            expandedAlpha = 0.0f;
        } else if (offsetPx >= crossFadeMaxHeight) {
            peekAlpha = 0.0f;
            expandedAlpha = 1.0f;
        } else if (offsetPx < midpoint) {
            peekAlpha = 1.0f - (offsetPx - mPeekHeight) / (midpoint - mPeekHeight);
            expandedAlpha = 0.0f;
        } else {
            peekAlpha = 0.0f;
            expandedAlpha = (offsetPx - midpoint) / (crossFadeMaxHeight - midpoint);
        }

        mModel.set(TabBottomSheetProperties.PEEK_STATE_ALPHA, peekAlpha);
        mModel.set(TabBottomSheetProperties.EXPANDED_STATE_ALPHA, expandedAlpha);
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

    @SheetState
    int getSheetStateForTesting() {
        return mCurrentSheetState;
    }
}
