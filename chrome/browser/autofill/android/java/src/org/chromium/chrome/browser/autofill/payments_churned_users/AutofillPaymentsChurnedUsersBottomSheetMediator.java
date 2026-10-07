// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator handling business logic for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetMediator implements BottomSheetObserver {
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate mDelegate;
    private boolean mIsAccepted;
    private boolean mIsCanceled;
    private boolean mIsDestroyed;

    AutofillPaymentsChurnedUsersBottomSheetMediator(
            BottomSheetController bottomSheetController,
            BottomSheetContent bottomSheetContent,
            PropertyModel model,
            AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate delegate) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mDelegate = delegate;
        model.set(
                AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED,
                this::onAcceptClicked);
        model.set(
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
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
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
        destroy();
    }

    void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mBottomSheetController.removeObserver(this);
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ false, StateChangeReason.NONE);
    }
}
