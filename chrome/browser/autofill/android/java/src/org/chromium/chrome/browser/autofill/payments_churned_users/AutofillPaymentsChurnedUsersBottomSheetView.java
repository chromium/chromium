// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;

/** View holder for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetView {
    private final View mContentView;
    private final TextView mTitleText;

    AutofillPaymentsChurnedUsersBottomSheetView(Context context) {
        mContentView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.autofill_payments_churned_users_bottom_sheet,
                                /* root= */ null);
        mTitleText = mContentView.findViewById(R.id.payments_churned_users_title);
    }

    View getContentView() {
        return mContentView;
    }

    TextView getTitleText() {
        return mTitleText;
    }
}
