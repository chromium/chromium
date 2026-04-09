// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/** Mediator for the Accessibility Annotator bottom sheet. */
@NullMarked
/*package*/ class AccessibilityAnnotatorBottomSheetMediator {
    private final BottomSheetController mBottomSheetController;
    private final AccessibilityAnnotatorBottomSheetContent mContent;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    mBottomSheetController.removeObserver(this);
                }
            };

    AccessibilityAnnotatorBottomSheetMediator(
            BottomSheetController bottomSheetController,
            AccessibilityAnnotatorBottomSheetContent content) {
        mBottomSheetController = bottomSheetController;
        mContent = content;
    }

    /** Requests to show the bottom sheet. */
    void requestShowContent() {
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
    }

    /** Handles the primary button click. */
    void onPrimaryButtonClicked() {
        hide(StateChangeReason.INTERACTION_COMPLETE);
    }

    /** Handles the secondary button click. */
    void onSecondaryButtonClicked() {
        hide(StateChangeReason.INTERACTION_COMPLETE);
    }

    /** Handles the learn more link click. */
    void onLearnMoreClicked() {
        hide(StateChangeReason.INTERACTION_COMPLETE);
    }

    /** Hides the bottom sheet. */
    void hide(@StateChangeReason int hideReason) {
        mBottomSheetController.hideContent(mContent, /* animate= */ true, hideReason);
    }
}
