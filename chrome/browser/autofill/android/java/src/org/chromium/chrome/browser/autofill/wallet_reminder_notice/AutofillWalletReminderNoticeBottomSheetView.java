// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;

/** View holder for the Wallet Reminder Notice bottom sheet. */
@NullMarked
/*package*/ class AutofillWalletReminderNoticeBottomSheetView {
    private final View mContentView;
    private final ImageView mHeaderIcon;
    private final TextView mTitleText;
    private final Button mGotItButton;

    AutofillWalletReminderNoticeBottomSheetView(Context context) {
        mContentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.autofill_wallet_reminder_notice_bottom_sheet, null);
        mHeaderIcon = mContentView.findViewById(R.id.wallet_reminder_header_icon);
        mTitleText = mContentView.findViewById(R.id.wallet_reminder_title);
        mGotItButton = mContentView.findViewById(R.id.wallet_reminder_button_got_it);
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

    Button getGotItButton() {
        return mGotItButton;
    }
}
