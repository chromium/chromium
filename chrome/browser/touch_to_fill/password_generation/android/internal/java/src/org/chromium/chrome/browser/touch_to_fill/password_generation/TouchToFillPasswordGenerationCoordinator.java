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
    private final TouchToFillPasswordGenerationView mTouchToFillPasswordGenerationView;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            hide();
        }
    };

    public TouchToFillPasswordGenerationCoordinator(
            Context context, BottomSheetController bottomSheetController) {
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
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.hideContent(mTouchToFillPasswordGenerationView, true);
    }

    @VisibleForTesting
    TouchToFillPasswordGenerationView getViewForTesting() {
        return mTouchToFillPasswordGenerationView;
    }
}
