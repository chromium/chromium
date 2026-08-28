// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator handling business logic for the Wallet Reminder Notice bottom sheet. */
@NullMarked
/*package*/ class AutofillWalletReminderNoticeBottomSheetMediator implements BottomSheetObserver {
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final PropertyModel mModel;
    private boolean mIsDestroyed;

    AutofillWalletReminderNoticeBottomSheetMediator(
            BottomSheetController bottomSheetController,
            BottomSheetContent bottomSheetContent,
            PropertyModel model) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mModel = model;
        mModel.set(
                AutofillWalletReminderNoticeBottomSheetProperties.ON_GOT_IT_CLICK_ACTION,
                this::onGotItClicked);
        mBottomSheetController.addObserver(this);
    }

    void requestShowContent() {
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    void onGotItClicked() {
        if (mIsDestroyed) {
            return;
        }
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
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
