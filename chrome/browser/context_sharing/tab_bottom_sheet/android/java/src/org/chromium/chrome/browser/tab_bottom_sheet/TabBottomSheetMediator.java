// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for tab bottom sheet */
@NullMarked
public class TabBottomSheetMediator {
    private final TouchArbitrator mTouchArbitrator;
    private final PropertyModel mModel;
    private final float mFullheightRatio;

    private @SheetState int mCurrentSheetState = SheetState.HIDDEN;

    public TabBottomSheetMediator(PropertyModel model) {
        mModel = model;
        mTouchArbitrator = new TouchArbitrator();
        // Setting statically right now, can be modified later to be set dynamically based on device
        // type.
        mFullheightRatio = 0.7f;
    }

    void onSheetStateChanged(@SheetState int state) {
        mCurrentSheetState = state;
    }

    /**
     * Sets the sheets height to 70% of the bottom sheet container height. If the bottom sheet had
     * never been called before, BottomSheetController.getContainerHeight() returns 0. To avoid this
     * we set the height after the sheet has been initialized. TODO(crbug.com/486916366): Temporary
     * fix until bottom sheet resizing is implemented.
     */
    void setMaxSheetHeight(int maxSheetHeight) {
        mModel.set(
                TabBottomSheetProperties.SHEET_HEIGHT,
                Math.round(maxSheetHeight * mFullheightRatio));
    }

    /** Returns the touch handler for the WebUI container. */
    TabBottomSheetWebUiContainer.TouchHandler getWebUiTouchHandler() {
        return mTouchArbitrator;
    }

    private boolean isShowing() {
        return mCurrentSheetState != SheetState.HIDDEN;
    }

    /** Inner class that arbitrates between scrolling the content and dragging the bottom sheet. */
    private class TouchArbitrator implements TabBottomSheetWebUiContainer.TouchHandler {
        @Override
        public boolean handleTouchEvent(TabBottomSheetWebUiContainer v, MotionEvent e) {
            if (!isShowing()) return false;

            // Minimal implementation for Stage 1: Always allow intercept by parent if not
            // specifically handled.
            if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
                v.getParent().requestDisallowInterceptTouchEvent(true);
            }
            return false;
        }
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
}
