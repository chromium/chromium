// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Mediator handling business logic for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetMediator implements BottomSheetObserver {
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private boolean mIsDestroyed;

    AutofillPaymentsChurnedUsersBottomSheetMediator(
            BottomSheetController bottomSheetController, BottomSheetContent bottomSheetContent) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mBottomSheetController.addObserver(this);
    }

    void requestShowContent() {
        if (mIsDestroyed) {
            return;
        }
        if (!mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            destroy();
        }
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        destroy();
    }

    void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ false, StateChangeReason.NONE);
        mBottomSheetController.removeObserver(this);
    }
}
