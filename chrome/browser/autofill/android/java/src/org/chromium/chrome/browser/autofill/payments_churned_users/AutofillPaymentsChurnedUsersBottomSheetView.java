// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.ui.widget.ButtonCompat;

/** View holder for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetView {
    private final View mContentView;
    private final ImageView mHeaderIcon;
    private final TextView mTitleText;
    private final TextView mDescriptionText;
    private final ButtonCompat mAcceptButton;
    private final ButtonCompat mCancelButton;

    AutofillPaymentsChurnedUsersBottomSheetView(Context context) {
        mContentView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.autofill_payments_churned_users_bottom_sheet,
                                /* root= */ null);
        mHeaderIcon = mContentView.findViewById(R.id.payments_churned_users_header_icon);
        mTitleText = mContentView.findViewById(R.id.payments_churned_users_title);
        mDescriptionText = mContentView.findViewById(R.id.payments_churned_users_description);
        mAcceptButton = mContentView.findViewById(R.id.payments_churned_users_accept_button);
        mCancelButton = mContentView.findViewById(R.id.payments_churned_users_cancel_button);
    }

    View getContentView() {
        return mContentView;
    }

    ImageView getHeaderIcon() {
        return mHeaderIcon;
    }

    TextView getTitleText() {
        return mTitleText;
    }

    TextView getDescriptionText() {
        return mDescriptionText;
    }

    ButtonCompat getAcceptButton() {
        return mAcceptButton;
    }

    ButtonCompat getCancelButton() {
        return mCancelButton;
    }
}
