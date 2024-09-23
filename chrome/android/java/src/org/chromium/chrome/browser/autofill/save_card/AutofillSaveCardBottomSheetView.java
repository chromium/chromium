// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

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

/*package*/ class AutofillSaveCardBottomSheetView {
    /** The view that contains all other views. */
    final ViewGroup mContentView;

    /** The view that optionally scrolls the contents on smaller screens. */
    final ScrollView mScrollView;

    /** The title of the bottom sheet UI. */
    final TextView mTitle;

    /** The text that describes what a save card does. */
    final TextView mDescription;

    /** The icon logo for a server upload save card. */
    final ImageView mLogoIcon;

    /** The view for the card icon, label, and description. */
    final View mCardView;

    /** The icon for the card. */
    final ImageView mCardIcon;

    /** The label for the card. */
    final TextView mCardLabel;

    /** The sub-label for the card. */
    final TextView mCardSubLabel;

    /** Legal messages. */
    final TextView mLegalMessage;

    /** The button that accepts the card save prompt. */
    final Button mAcceptButton;

    /** The button that declines the card save prompt. */
    final Button mCancelButton;

    /**
     * Contains the loading view. Needed for proper a11y announcement of the content description.
     */
    final View mLoadingViewContainer;

    /** The view shown while the card is being uploaded. */
    final LoadingView mLoadingView;

    AutofillSaveCardBottomSheetView(Context context) {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.autofill_save_card_bottom_sheet, /* root= */ null);
        mScrollView = mContentView.findViewById(R.id.autofill_save_card_scroll_view);
        mTitle = mContentView.findViewById(R.id.autofill_save_card_title_text);
        mDescription = mContentView.findViewById(R.id.autofill_save_card_description_text);
        mLogoIcon = mContentView.findViewById(R.id.autofill_save_card_icon);
        mCardView = mContentView.findViewById(R.id.autofill_credit_card_chip);
        mCardIcon = mContentView.findViewById(R.id.autofill_save_card_credit_card_icon);
        mCardLabel = mContentView.findViewById(R.id.autofill_save_card_credit_card_label);
        mCardSubLabel = mContentView.findViewById(R.id.autofill_save_card_credit_card_sublabel);
        mLegalMessage = mContentView.findViewById(R.id.legal_message);
        mAcceptButton = mContentView.findViewById(R.id.autofill_save_card_confirm_button);
        mCancelButton = mContentView.findViewById(R.id.autofill_save_card_cancel_button);
        mLoadingViewContainer =
                mContentView.findViewById(R.id.autofill_save_card_loading_view_container);
        mLoadingView = mContentView.findViewById(R.id.autofill_save_card_loading_view);
    }
}
