// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.widget.LoadingView;

/** The view of the autofill virtual card enrollment bottom sheet UI. */
/*package*/ class AutofillVcnEnrollBottomSheetView {
    /** The view that contains all other views. */
    final ViewGroup mContentView;

    /** The view that optionally scrolls the contents on smaller screens. */
    final ScrollView mScrollView;

    /** The title of the bottom sheet UI. */
    final TextView mDialogTitle;

    /** The text that describes what a virtual card does. */
    final TextView mVirtualCardDescription;

    /** The container for the card icon, label, and description. */
    final View mCardContainer;

    /** The icon for the card. */
    final ImageView mIssuerIcon;

    /** The label for the card. */
    final TextView mCardLabel;

    /** The description for the card. */
    final TextView mCardDescription;

    /** Legal messages from Google Pay. */
    final TextView mGoogleLegalMessage;

    /** Legal messages from the issuer bank. */
    final TextView mIssuerLegalMessage;

    /** The button that accepts the enrollment prompt. */
    final Button mAcceptButton;

    /** The button that cancels the enrollment. */
    final Button mCancelButton;

    /**
     * Contains the loading view. Needed for proper a11y announcement of the content description.
     */
    final View mLoadingViewContainer;

    /** The view shown while enrolling the card. */
    final LoadingView mLoadingView;

    /**
     * Creates the view of the autofill virtual card enrollment bottom sheet UI.
     *
     * @param context The context for inflating the UI layout XML file.
     */
    AutofillVcnEnrollBottomSheetView(Context context) {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.autofill_vcn_enroll_bottom_sheet_content, null);
        mScrollView = mContentView.findViewById(R.id.scroll_view);
        mDialogTitle = mContentView.findViewById(R.id.dialog_title);
        mVirtualCardDescription = mContentView.findViewById(R.id.virtual_card_description);
        mCardContainer = mContentView.findViewById(R.id.card_container);
        mIssuerIcon = mContentView.findViewById(R.id.issuer_icon);
        mCardLabel = mContentView.findViewById(R.id.card_label);
        mCardDescription = mContentView.findViewById(R.id.card_description);
        mGoogleLegalMessage = mContentView.findViewById(R.id.google_legal_message);
        mIssuerLegalMessage = mContentView.findViewById(R.id.issuer_legal_message);
        mAcceptButton = mContentView.findViewById(R.id.accept_button);
        mCancelButton = mContentView.findViewById(R.id.cancel_button);
        mLoadingViewContainer = mContentView.findViewById(R.id.loading_view_container);
        mLoadingView = mContentView.findViewById(R.id.loading_view);
    }
}
