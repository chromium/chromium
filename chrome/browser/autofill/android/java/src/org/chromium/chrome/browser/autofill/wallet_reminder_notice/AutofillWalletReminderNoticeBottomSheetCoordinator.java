// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for assembling and controlling the Wallet Reminder Notice bottom sheet. */
@NullMarked
public class AutofillWalletReminderNoticeBottomSheetCoordinator {
    private final AutofillWalletReminderNoticeBottomSheetMediator mMediator;
    private final AutofillWalletReminderNoticeBottomSheetView mView;
    private final PropertyModel mModel;
    private @Nullable
            PropertyModelChangeProcessor<
                    PropertyModel, AutofillWalletReminderNoticeBottomSheetView, PropertyKey>
            mModelChangeProcessor;

    public AutofillWalletReminderNoticeBottomSheetCoordinator(
            Context context, BottomSheetController bottomSheetController) {
        mView = new AutofillWalletReminderNoticeBottomSheetView(context);

        AutofillWalletReminderNoticeBottomSheetContent content =
                new AutofillWalletReminderNoticeBottomSheetContent(mView.getContentView());

        mModel =
                new PropertyModel.Builder(
                                AutofillWalletReminderNoticeBottomSheetProperties.ALL_KEYS)
                        .with(
                                AutofillWalletReminderNoticeBottomSheetProperties.TITLE,
                                context.getString(R.string.autofill_wallet_reminder_notice_title))
                        .build();

        mMediator =
                new AutofillWalletReminderNoticeBottomSheetMediator(
                        bottomSheetController, content, mModel);

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mView, AutofillWalletReminderNoticeBottomSheetViewBinder::bind);
    }

    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    public void destroy() {
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }
        mMediator.destroy();
    }

    PropertyModel getPropertyModelForTesting() {
        return mModel;
    }

    View getContentViewForTesting() {
        return mView.getContentView();
    }
}
