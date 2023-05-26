// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * Coordinates the password generation bottom sheet functionality. It shows the bottom sheet, fills
 * in the generated password and handles the bottom sheet dismissal.
 */
class TouchToFillPasswordGenerationCoordinator {
    interface Delegate {
        /** Called when the bottom sheet is hidden (both by user and programmatically). */
        void onDismissed();
    }

    private final TouchToFillPasswordGenerationView mTouchToFillPasswordGenerationView;
    private final Delegate mTouchToFillPasswordGenerationDelegate;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            onDismissed(reason);
        }
    };

    public TouchToFillPasswordGenerationCoordinator(Context context,
            BottomSheetController bottomSheetController,
            Delegate touchToFillPasswordGenerationDelegate) {
        mTouchToFillPasswordGenerationDelegate = touchToFillPasswordGenerationDelegate;
        mBottomSheetController = bottomSheetController;
        mTouchToFillPasswordGenerationView = new TouchToFillPasswordGenerationView(context);
    }

    /**
     *  Displays the bottom sheet.
     */
    public boolean show() {
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (mBottomSheetController.requestShowContent(mTouchToFillPasswordGenerationView, true)) {
            return true;
        }
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        return false;
    }

    public void hide() {
        onDismissed(StateChangeReason.NONE);
    }

    private void onDismissed(@StateChangeReason int reason) {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.hideContent(mTouchToFillPasswordGenerationView, true);
        mTouchToFillPasswordGenerationDelegate.onDismissed();
    }

    @VisibleForTesting
    TouchToFillPasswordGenerationView getViewForTesting() {
        return mTouchToFillPasswordGenerationView;
    }
}
