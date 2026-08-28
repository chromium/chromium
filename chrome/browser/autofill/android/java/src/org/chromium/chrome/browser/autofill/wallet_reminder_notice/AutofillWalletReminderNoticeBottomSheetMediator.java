// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.payments.WalletReminderNoticeInteraction;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator handling business logic for the Wallet Reminder Notice bottom sheet. */
@NullMarked
/*package*/ class AutofillWalletReminderNoticeBottomSheetMediator implements BottomSheetObserver {
    @VisibleForTesting
    static final String HISTOGRAM_INTERACTION = "Autofill.WalletReminderNotice.Interaction";

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final PropertyModel mModel;
    private boolean mAcceptButtonClicked;
    private boolean mLinkClicked;
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
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_INTERACTION,
                WalletReminderNoticeInteraction.ACKNOWLEDGED_CTA,
                WalletReminderNoticeInteraction.MAX_VALUE + 1);
        mAcceptButtonClicked = true;
        mBottomSheetController.hideContent(
                mBottomSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
    }

    void onLegalMessageLinkClicked() {
        if (mIsDestroyed) {
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_INTERACTION,
                WalletReminderNoticeInteraction.CLICKED_LINK,
                WalletReminderNoticeInteraction.MAX_VALUE + 1);
        mLinkClicked = true;
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        boolean dismissedByUser =
                reason == StateChangeReason.SWIPE
                        || reason == StateChangeReason.BACK_PRESS
                        || reason == StateChangeReason.TAP_SCRIM;
        if (!mAcceptButtonClicked && !mLinkClicked && dismissedByUser) {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_INTERACTION,
                    WalletReminderNoticeInteraction.DISMISSED,
                    WalletReminderNoticeInteraction.MAX_VALUE + 1);
        }
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
