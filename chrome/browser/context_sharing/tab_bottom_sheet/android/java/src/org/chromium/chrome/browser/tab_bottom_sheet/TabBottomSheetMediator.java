// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;

/** Mediator for tab bottom sheet */
@NullMarked
public class TabBottomSheetMediator {
    private final TouchArbitrator mTouchArbitrator;

    private @SheetState int mCurrentSheetState = SheetState.HIDDEN;

    public TabBottomSheetMediator() {
        mTouchArbitrator = new TouchArbitrator();
    }

    void onSheetStateChanged(@SheetState int state) {
        mCurrentSheetState = state;
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
}
