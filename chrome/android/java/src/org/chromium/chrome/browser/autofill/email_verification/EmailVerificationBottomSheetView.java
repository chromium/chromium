// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

/** View component for the email verification bottom sheet. */
@NullMarked
/*package*/ class EmailVerificationBottomSheetView {
    /** The view that contains all other views. */
    final ViewGroup mContentView;

    /** The drag handler of the bottom sheet. */
    final ImageView mDragHandler;

    /** The view that optionally scrolls the contents on smaller screens. */
    final ScrollView mScrollView;

    /** The email icon logo. */
    final ImageView mIcon;

    /** The title of the bottom sheet UI. */
    final TextView mTitle;

    /** The text that describes what email verification does. */
    final TextView mDescription;

    /** The button that confirms/accepts email verification. */
    final Button mConfirmButton;

    /** The button that declines/cancels email verification. */
    final Button mCancelButton;

    EmailVerificationBottomSheetView(Context context) {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.email_verification_bottom_sheet, /* root= */ null);
        mDragHandler = mContentView.findViewById(R.id.email_verification_drag_handler);
        mScrollView = mContentView.findViewById(R.id.email_verification_scroll_view);
        mIcon = mContentView.findViewById(R.id.email_verification_icon);
        mTitle = mContentView.findViewById(R.id.email_verification_title_text);
        mDescription = mContentView.findViewById(R.id.email_verification_description_text);
        mConfirmButton = mContentView.findViewById(R.id.email_verification_confirm_button);
        mCancelButton = mContentView.findViewById(R.id.email_verification_cancel_button);
    }
}
