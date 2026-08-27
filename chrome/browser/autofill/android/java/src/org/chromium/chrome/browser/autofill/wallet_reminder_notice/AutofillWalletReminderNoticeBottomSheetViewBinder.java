// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import static org.chromium.chrome.browser.autofill.wallet_reminder_notice.AutofillWalletReminderNoticeBottomSheetProperties.HEADER_ICON;
import static org.chromium.chrome.browser.autofill.wallet_reminder_notice.AutofillWalletReminderNoticeBottomSheetProperties.ON_GOT_IT_CLICK_ACTION;
import static org.chromium.chrome.browser.autofill.wallet_reminder_notice.AutofillWalletReminderNoticeBottomSheetProperties.TITLE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
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
