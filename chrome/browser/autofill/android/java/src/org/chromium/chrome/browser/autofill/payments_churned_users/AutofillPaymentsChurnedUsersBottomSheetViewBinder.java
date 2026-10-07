// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.ACCEPT_BUTTON_LABEL;
import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.CANCEL_BUTTON_LABEL;
import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.DESCRIPTION;
import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.HEADER_ICON;
import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED;
import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.ON_CANCEL_CLICKED;
import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.SHOW_LOADING_STATE;
import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.TITLE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetViewBinder {
    static void bind(
            PropertyModel model,
            AutofillPaymentsChurnedUsersBottomSheetView view,
            PropertyKey propertyKey) {
        if (propertyKey == TITLE) {
            view.getTitleText().setText(model.get(TITLE));
        } else if (propertyKey == DESCRIPTION) {
            view.getDescriptionText().setText(model.get(DESCRIPTION));
        } else if (propertyKey == HEADER_ICON) {
            int iconRes = model.get(HEADER_ICON);
            if (iconRes != 0) {
                view.getHeaderIcon().setImageResource(iconRes);
                view.getHeaderIcon().setVisibility(View.VISIBLE);
            } else {
                view.getHeaderIcon().setVisibility(View.GONE);
            }
        } else if (propertyKey == ACCEPT_BUTTON_LABEL) {
            view.getAcceptButton().setText(model.get(ACCEPT_BUTTON_LABEL));
        } else if (propertyKey == CANCEL_BUTTON_LABEL) {
            view.getCancelButton().setText(model.get(CANCEL_BUTTON_LABEL));
        } else if (propertyKey == ON_ACCEPT_CLICKED) {
            Runnable action = model.get(ON_ACCEPT_CLICKED);
            view.getAcceptButton()
                    .setOnClickListener(
                            _ -> {
                                if (action != null) {
                                    action.run();
                                }
                            });
        } else if (propertyKey == ON_CANCEL_CLICKED) {
            Runnable action = model.get(ON_CANCEL_CLICKED);
            view.getCancelButton()
                    .setOnClickListener(
                            _ -> {
                                if (action != null) {
                                    action.run();
                                }
                            });
        } else if (propertyKey == SHOW_LOADING_STATE) {
            boolean showLoading = model.get(SHOW_LOADING_STATE);
            int buttonVisibility = showLoading ? View.GONE : View.VISIBLE;
            view.getAcceptButton().setVisibility(buttonVisibility);
            view.getCancelButton().setVisibility(buttonVisibility);
            view.getLoadingSpinner().setVisibility(showLoading ? View.VISIBLE : View.GONE);
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    private AutofillPaymentsChurnedUsersBottomSheetViewBinder() {}
}
