// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.chrome.R;

/** This class inflates the layout for the Autofill save IBAN bottom sheet. */
/*package*/ class AutofillSaveIbanBottomSheetView {
    /** The view that contains all other views. */
    final ViewGroup mContentView;

    /** The view that optionally scrolls the contents on smaller screens. */
    final ScrollView mScrollView;

    /** The obfuscated value for IBAN. */
    final TextView mIbanLabel;

    AutofillSaveIbanBottomSheetView(Context context) {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.autofill_save_iban_bottom_sheet, null);
        mScrollView = mContentView.findViewById(R.id.autofill_save_iban_scroll_view);
        mIbanLabel = mContentView.findViewById(R.id.autofill_save_iban_label);
    }
}
