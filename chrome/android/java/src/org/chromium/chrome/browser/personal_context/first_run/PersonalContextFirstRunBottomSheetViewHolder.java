// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.personal_context.first_run;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.RawRes;

import com.airbnb.lottie.LottieAnimationView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** View for the Personal Context first-run bottom sheet. */
@NullMarked
/*package*/ class PersonalContextFirstRunBottomSheetViewHolder {
    final ViewGroup mContentView;
    final ScrollView mScrollView;
    final LottieAnimationView mIcon;
    final TextView mTitle;
    final TextView mDescription;
    final TextViewWithClickableSpans mLearnMoreDescription;
    final TextView mCard1Text;
    final TextView mCard2Text;
    final Button mPrimaryButton;
    final Button mSecondaryButton;

    PersonalContextFirstRunBottomSheetViewHolder(Context context) {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.personal_context_first_run_bottom_sheet,
                                        /* root= */ null);
        mScrollView = mContentView.findViewById(R.id.personal_context_scroll_view);
        mIcon = mContentView.findViewById(R.id.personal_context_icon);
        mTitle = mContentView.findViewById(R.id.personal_context_title);
        mDescription = mContentView.findViewById(R.id.personal_context_description);
        mLearnMoreDescription =
                mContentView.findViewById(R.id.personal_context_learn_more_description);
        mCard1Text = mContentView.findViewById(R.id.personal_context_card_1_text);
        mCard2Text = mContentView.findViewById(R.id.personal_context_card_2_text);
        mPrimaryButton = mContentView.findViewById(R.id.personal_context_primary_button);
        mSecondaryButton = mContentView.findViewById(R.id.personal_context_secondary_button);

        mLearnMoreDescription.setMovementMethod(LinkMovementMethod.getInstance());
    }

    void playAnimation() {
        mIcon.playAnimation();
    }

    void setAnimation(@RawRes int resId) {
        mIcon.setAnimation(resId);
    }
}
