// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.chrome.R;

/** The view of the autofill virtual card enrollment bottom sheet UI. */
/*package*/ class AutofillVcnEnrollBottomSheetView {
    /** The view that contains all other views. */
    /*package*/ final ViewGroup mContentView;

    /** The title of the bottom sheet UI. */
    /*package*/ final TextView mDialogTitle;

    /** The button that accepts the enrollment prompt. */
    /*package*/ final Button mAcceptButton;

    /** The button that cancels the enrollment. */
    /*package*/ final Button mCancelButton;

    /**
     * Creates the view of the autofill virtual card enrollment bottom sheet UI.
     *
     * @param context The context for inflating the UI layout XML file.
     */
    /*package*/ AutofillVcnEnrollBottomSheetView(Context context) {
        mContentView = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.autofill_vcn_enroll_bottom_sheet_content, null);
        mDialogTitle = mContentView.findViewById(R.id.dialog_title);
        mAcceptButton = mContentView.findViewById(R.id.accept_button);
        mCancelButton = mContentView.findViewById(R.id.cancel_button);
    }
}
