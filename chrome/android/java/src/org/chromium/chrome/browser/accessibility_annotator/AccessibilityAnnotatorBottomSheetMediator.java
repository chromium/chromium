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
    private final AccessibilityAnnotatorBottomSheetCoordinator.Delegate mDelegate;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    if (reason != StateChangeReason.INTERACTION_COMPLETE) {
                        mDelegate.onInfoDismissed();
                    }
                    mBottomSheetController.removeObserver(this);
                }
            };

    AccessibilityAnnotatorBottomSheetMediator(
            BottomSheetController bottomSheetController,
            AccessibilityAnnotatorBottomSheetContent content,
            AccessibilityAnnotatorBottomSheetCoordinator.Delegate delegate) {
        mBottomSheetController = bottomSheetController;
        mContent = content;
        mDelegate = delegate;
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @return True if the content was shown, false if it was suppressed.
     */
    boolean requestShowContent() {
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
            return false;
        }
        return true;
    }

    /** Handles the acknowledge action. */
    void onAcknowledgeClicked() {
        mDelegate.onInfoAcknowledged();
        hide(StateChangeReason.INTERACTION_COMPLETE);
    }

    /** Handles the manage settings action. */
    void onManageSettingsClicked() {
        mDelegate.onManageSettingsClicked();
    }

    /** Handles the learn more link click. */
    void onLearnMoreClicked() {
        mDelegate.onLearnMoreClicked();
    }

    /** Hides the bottom sheet. */
    void hide(@StateChangeReason int hideReason) {
        mBottomSheetController.hideContent(mContent, /* animate= */ true, hideReason);
    }
}
