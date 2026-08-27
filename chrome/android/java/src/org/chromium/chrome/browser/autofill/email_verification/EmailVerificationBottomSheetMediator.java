// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillSheetUiController;
import org.chromium.chrome.browser.autofill.AutofillSheetUiControllerFactory;
import org.chromium.components.autofill.EmailVerificationPermissionUiStatus;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class for the email verification UI.
 *
 * <p>This component shows a bottom sheet to let the user confirm or decline automatic email
 * verification.
 *
 * <p>This mediator sends UI events (onUiShown, onUiDecision) to the delegate.
 */
@NullMarked
/*package*/ class EmailVerificationBottomSheetMediator implements BottomSheetObserver {
    private final EmailVerificationBottomSheetContent mContent;
    private final AutofillSheetUiController mUiController;
    private final EmailVerificationBottomSheetCoordinator.Delegate mDelegate;
    private final PropertyModel mModel;
    private boolean mActionTaken;

    /**
     * Creates the mediator.
     *
     * @param context The context for this component.
     * @param title The title text of the bottom sheet.
     * @param description The description text of the bottom sheet.
     * @param content The bottom sheet content to be shown.
     * @param uiController The controller to use for showing or hiding the content.
     * @param delegate The delegate to signal UI flow events to.
     */
    EmailVerificationBottomSheetMediator(
            Context context,
            String title,
            String description,
            EmailVerificationBottomSheetContent content,
            AutofillSheetUiController uiController,
            EmailVerificationBottomSheetCoordinator.Delegate delegate) {
        mContent = content;
        mUiController = uiController;
        mDelegate = delegate;

        String confirmButtonLabel =
                context.getString(R.string.autofill_email_verifier_prompt_verify);
        String cancelButtonLabel =
                context.getString(R.string.autofill_email_verifier_prompt_not_now);

        mModel =
                new PropertyModel.Builder(EmailVerificationBottomSheetProperties.ALL_KEYS)
                        .with(EmailVerificationBottomSheetProperties.TITLE, title)
                        .with(EmailVerificationBottomSheetProperties.DESCRIPTION, description)
                        .with(
                                EmailVerificationBottomSheetProperties.CONFIRM_BUTTON_LABEL,
                                confirmButtonLabel)
                        .with(
                                EmailVerificationBottomSheetProperties.CANCEL_BUTTON_LABEL,
                                cancelButtonLabel)
                        .with(
                                EmailVerificationBottomSheetProperties.DRAG_HANDLE_VISIBLE,
                                !AutofillSheetUiControllerFactory.shouldUseNonBlockingDialog(
                                        context))
                        .with(
                                EmailVerificationBottomSheetProperties.ON_CONFIRM_CLICKED,
                                this::onAccepted)
                        .with(
                                EmailVerificationBottomSheetProperties.ON_CANCEL_CLICKED,
                                this::onDeclined)
                        .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    /** Requests to show the bottom sheet content. */
    void requestShowContent() {
        if (mUiController.requestShowContent(mContent, /* animate= */ true)) {
            mUiController.addObserver(this);
            mDelegate.onUiShown();
        } else {
            onDecision(EmailVerificationPermissionUiStatus.OTHER);
        }
    }

    public void onAccepted() {
        onDecision(EmailVerificationPermissionUiStatus.ALLOWED);
    }

    public void onDeclined() {
        onDecision(EmailVerificationPermissionUiStatus.DECLINED);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        switch (reason) {
            case StateChangeReason.BACK_PRESS:
            case StateChangeReason.SWIPE:
            case StateChangeReason.TAP_SCRIM:
                onDecision(EmailVerificationPermissionUiStatus.USER_ABORTED);
                break;
            case StateChangeReason.NAVIGATION:
            case StateChangeReason.COMPOSITED_UI:
            case StateChangeReason.VR:
            case StateChangeReason.PROMOTE_TAB:
            case StateChangeReason.OMNIBOX_FOCUS:
                onDecision(EmailVerificationPermissionUiStatus.TAB_GONE);
                break;
            case StateChangeReason.INTERACTION_COMPLETE:
                // Handled by onAccepted() / onDeclined().
                break;
            default:
                onDecision(EmailVerificationPermissionUiStatus.OTHER);
                break;
        }
    }

    private void onDecision(@EmailVerificationPermissionUiStatus int status) {
        if (mActionTaken) return;
        mActionTaken = true;
        hide(StateChangeReason.INTERACTION_COMPLETE);
        mDelegate.onUiDecision(status);
    }

    /** Hide the bottom sheet (if showing). */
    void hide(@StateChangeReason int hideReason) {
        mUiController.removeObserver(this);
        mUiController.hideContent(mContent, /* animate= */ true, hideReason);
    }
}
