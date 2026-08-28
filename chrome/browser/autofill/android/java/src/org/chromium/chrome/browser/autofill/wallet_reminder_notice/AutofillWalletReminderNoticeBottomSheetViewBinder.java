// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import static org.chromium.chrome.browser.autofill.wallet_reminder_notice.AutofillWalletReminderNoticeBottomSheetProperties.HEADER_ICON;
import static org.chromium.chrome.browser.autofill.wallet_reminder_notice.AutofillWalletReminderNoticeBottomSheetProperties.LEGAL_MESSAGE;
import static org.chromium.chrome.browser.autofill.wallet_reminder_notice.AutofillWalletReminderNoticeBottomSheetProperties.ON_GOT_IT_CLICK_ACTION;
import static org.chromium.chrome.browser.autofill.wallet_reminder_notice.AutofillWalletReminderNoticeBottomSheetProperties.TITLE;

import android.text.SpannableStringBuilder;
import android.text.method.LinkMovementMethod;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.components.autofill.payments.LegalMessage;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the Wallet Reminder Notice bottom sheet. */
@NullMarked
/*package*/ class AutofillWalletReminderNoticeBottomSheetViewBinder {
    static void bind(
            PropertyModel model,
            AutofillWalletReminderNoticeBottomSheetView view,
            PropertyKey propertyKey) {
        if (propertyKey == TITLE) {
            view.getTitleText().setText(model.get(TITLE));
        } else if (propertyKey == HEADER_ICON) {
            int iconRes = model.get(HEADER_ICON);
            if (iconRes != 0) {
                view.getHeaderIcon().setImageResource(iconRes);
                view.getHeaderIcon().setVisibility(View.VISIBLE);
            } else {
                view.getHeaderIcon().setVisibility(View.GONE);
            }
        } else if (propertyKey == LEGAL_MESSAGE) {
            LegalMessage legalMessage = model.get(LEGAL_MESSAGE);
            if (legalMessage == null || legalMessage.mLines.isEmpty()) {
                view.getLegalMessage().setVisibility(View.GONE);
                return;
            }
            SpannableStringBuilder stringBuilder =
                    AutofillUiUtils.getSpannableStringForLegalMessageLines(
                            view.getContentView().getContext(),
                            legalMessage.mLines,
                            /* underlineLinks= */ true,
                            legalMessage.mLink::accept);
            view.getLegalMessage().setText(stringBuilder);
            view.getLegalMessage().setVisibility(View.VISIBLE);
            view.getLegalMessage().setMovementMethod(LinkMovementMethod.getInstance());
        } else if (propertyKey == ON_GOT_IT_CLICK_ACTION) {
            Runnable action = model.get(ON_GOT_IT_CLICK_ACTION);
            view.getGotItButton()
                    .setOnClickListener(
                            v -> {
                                if (action != null) {
                                    action.run();
                                }
                            });
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    private AutofillWalletReminderNoticeBottomSheetViewBinder() {}
}
