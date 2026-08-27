// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.AutofillSheetUiController;
import org.chromium.chrome.browser.autofill.AutofillSheetUiControllerFactory;
import org.chromium.chrome.browser.autofill.anchored_dialog.AnchoredDialogCoordinator;
import org.chromium.components.autofill.EmailVerificationPermissionUiStatus;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the autofill email verification UI.
 *
 * <p>This component shows a bottom sheet asking the user for permission to verify their email
 * address automatically on supported sites.
 */
@NullMarked
public class EmailVerificationBottomSheetCoordinator {
    /** Native/controller callbacks from the email verification bottom sheet. */
    public interface Delegate {
        /** Called when the bottom sheet is shown to the user. */
        void onUiShown();

        /** Called when a UI decision is made. */
        void onUiDecision(@EmailVerificationPermissionUiStatus int status);
    }

    private final EmailVerificationBottomSheetView mView;
    private final EmailVerificationBottomSheetMediator mMediator;

    /**
     * Creates the coordinator.
     *
     * @param context The context for this component.
     * @param title The title text of the bottom sheet.
     * @param description The description text of the bottom sheet.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     *     shown.
     * @param anchoredDialogCoordinator The anchored dialog coordinator where this bottom sheet will
     *     be shown.
     * @param delegate The callbacks for user actions.
     */
    public EmailVerificationBottomSheetCoordinator(
            Context context,
            String title,
            String description,
            BottomSheetController bottomSheetController,
            AnchoredDialogCoordinator anchoredDialogCoordinator,
            Delegate delegate) {
        mView = new EmailVerificationBottomSheetView(context);

        AutofillSheetUiController uiController =
                AutofillSheetUiControllerFactory.createUiController(
                        context, bottomSheetController, anchoredDialogCoordinator);

        mMediator =
                new EmailVerificationBottomSheetMediator(
                        context,
                        title,
                        description,
                        new EmailVerificationBottomSheetContent(
                                mView.mContentView, mView.mScrollView),
                        uiController,
                        delegate);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, EmailVerificationBottomSheetViewBinder::bind);
    }

    /** Request to show the bottom sheet. */
    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    /** Hides this component, hiding the bottom sheet if needed. */
    public void hide(@StateChangeReason int hideReason) {
        mMediator.hide(hideReason);
    }

    /*package*/ EmailVerificationBottomSheetView getViewForTesting() {
        return mView;
    }

    /*package*/ PropertyModel getPropertyModelForTesting() {
        return mMediator.getModel();
    }

    /*package*/ EmailVerificationBottomSheetMediator getMediatorForTesting() {
        return mMediator;
    }
}
