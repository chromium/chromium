// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.os.Handler;
import android.os.Looper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator handling business logic for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetMediator implements BottomSheetObserver {
    /**
     * Delay in milliseconds before hiding the bottom sheet after the user clicks the accept ("Turn
     * on") button, matching Desktop's {@code kMillisecondsUntilConfirmationBubbleIsShown}.
     */
    // LINT.IfChange(ChurnedUsersLoadingDelayMs)
    static final long LOADING_DELAY_MS = 1000L;

    // LINT.ThenChange(//chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h:ChurnedUsersLoadingDelayMs)

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final PropertyModel mModel;
    private final AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate mDelegate;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private boolean mIsAccepted;
    private boolean mIsCanceled;
    private boolean mShouldShowConfirmation;
    private boolean mIsDestroyed;

    AutofillPaymentsChurnedUsersBottomSheetMediator(
            BottomSheetController bottomSheetController,
            BottomSheetContent bottomSheetContent,
            PropertyModel model,
            AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate delegate) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mModel = model;
        mDelegate = delegate;
        mModel.set(
                AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED,
                this::onAcceptClicked);
        mModel.set(
                AutofillPaymentsChurnedUsersBottomSheetProperties.ON_CANCEL_CLICKED,
                this::onCancelClicked);
    }

    void requestShowContent() {
        if (mIsDestroyed) {
            return;
        }
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            mBottomSheetController.addObserver(this);
        } else {
            mDelegate.onUiNotShown();
            destroy();
        }
    }

    void onAcceptClicked() {
        if (mIsDestroyed || mIsAccepted || mIsCanceled) {
            return;
        }
        mIsAccepted = true;
        mDelegate.onUiAccepted();
        mModel.set(AutofillPaymentsChurnedUsersBottomSheetProperties.SHOW_LOADING_STATE, true);
        mHandler.postDelayed(
                () -> {
                    if (mIsDestroyed) {
                        return;
                    }
                    mShouldShowConfirmation = true;
                    mBottomSheetController.hideContent(
                            mBottomSheetContent,
                            /* animate= */ true,
                            StateChangeReason.INTERACTION_COMPLETE);
                },
                LOADING_DELAY_MS);
    }

    void onCancelClicked() {
        if (mIsDestroyed || mIsAccepted || mIsCanceled) {
            return;
        }
        mIsCanceled = true;
        mDelegate.onUiCanceled();
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        if (mIsDestroyed) {
            return;
        }
        if (!mIsAccepted && !mIsCanceled) {
            mDelegate.onUiDismissed();
        }
        // When hiding after the 1,000ms loading spinner completes, wait for
        // onSheetStateChanged(SheetState.HIDDEN) so ChromeActivitySnackbarHelper pops
        // ParentOverrideSlot.BOTTOM_SHEET before the confirmation snackbar is shown.
        if (!mShouldShowConfirmation) {
            destroy();
        }
    }

    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        if (mIsDestroyed || !mShouldShowConfirmation || newState != SheetState.HIDDEN) {
            return;
        }
        mShouldShowConfirmation = false;
        mDelegate.onShowConfirmation();
        destroy();
    }

    void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mShouldShowConfirmation = false;
        mHandler.removeCallbacksAndMessages(null);
        mBottomSheetController.removeObserver(this);
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ false, StateChangeReason.NONE);
    }
}
