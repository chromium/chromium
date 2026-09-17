// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.chromium.chrome.browser.autofill.payments_churned_users.AutofillPaymentsChurnedUsersBottomSheetProperties.TITLE;

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
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    private AutofillPaymentsChurnedUsersBottomSheetViewBinder() {}
}
