// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/**
 * Mediator class for the autofill save card UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a payment card (either
 * locally or uploaded).
 *
 * <p>This mediator manages the lifecycle of the bottom sheet by observing layout and tab changes.
 * When the layout is no longer on browsing (for example the tab switcher) the bottom sheet is
 * hidden. When the selected tab changes the bottom sheet is hidden.
 *
 * <p>This mediator sends UI events (OnUiShown, OnUiAccepted, etc.) to the bridge.
 */
/*package*/ class AutofillSaveCardBottomSheetMediator
        implements AutofillSaveCardBottomSheetLifecycle.ControllerDelegate {
    private final AutofillSaveCardBottomSheetContent mContent;
    private final AutofillSaveCardBottomSheetLifecycle mLifecycle;
    private final BottomSheetController mBottomSheetController;
    private final AutofillSaveCardBottomSheetCoordinator.NativeDelegate mDelegate;

    /**
     * Creates the mediator.
     *
     * @param content The bottom sheet content to be shown.
     * @param lifecycle A custom lifecycle that ignores page navigation.
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param delegate The delegate to signal UI flow events (OnUiShown, OnUiAccepted, etc.) to.
     */
    AutofillSaveCardBottomSheetMediator(
            AutofillSaveCardBottomSheetContent content,
            AutofillSaveCardBottomSheetLifecycle lifecycle,
            BottomSheetController bottomSheetController,
            AutofillSaveCardBottomSheetCoordinator.NativeDelegate delegate) {
        mContent = content;
        mLifecycle = lifecycle;
        mBottomSheetController = bottomSheetController;
        mDelegate = delegate;
    }

    /** Requests to show the bottom sheet content. */
    void requestShowContent() {
        if (mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mLifecycle.begin(this);
            mDelegate.onUiShown();
        } else {
            mDelegate.onUiIgnored();
        }
    }

    public void onAccepted() {
        hide(StateChangeReason.INTERACTION_COMPLETE);
        mDelegate.onUiAccepted();
    }

    @Override
    public void onCanceled() {
        hide(StateChangeReason.INTERACTION_COMPLETE);
        mDelegate.onUiCanceled();
    }

    @Override
    public void onIgnored() {
        hide(StateChangeReason.INTERACTION_COMPLETE);
        mDelegate.onUiIgnored();
    }

    /** Hide the bottom sheet (if showing) and end the lifecycle. */
    void hide(@StateChangeReason int hideReason) {
        mLifecycle.end();
        mBottomSheetController.hideContent(mContent, /* animate= */ true, hideReason);
    }
}
