// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

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

/** View for the Accessibility Annotator first-run bottom sheet. */
@NullMarked
/*package*/ class AccessibilityAnnotatorFirstRunBottomSheetViewHolder {
    final ViewGroup mContentView;
    final ScrollView mScrollView;
    final LottieAnimationView mIcon;
    final TextView mTitle;
    final TextViewWithClickableSpans mDescription;
    final Button mPrimaryButton;
    final Button mSecondaryButton;

    AccessibilityAnnotatorFirstRunBottomSheetViewHolder(Context context) {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.accessibility_annotator_first_run_bottom_sheet,
                                        /* root= */ null);
        mScrollView = mContentView.findViewById(R.id.accessibility_annotator_scroll_view);
        mIcon = mContentView.findViewById(R.id.accessibility_annotator_icon);
        mTitle = mContentView.findViewById(R.id.accessibility_annotator_title);
        mDescription = mContentView.findViewById(R.id.accessibility_annotator_description);
        mPrimaryButton = mContentView.findViewById(R.id.accessibility_annotator_primary_button);
        mSecondaryButton = mContentView.findViewById(R.id.accessibility_annotator_secondary_button);

        mDescription.setMovementMethod(LinkMovementMethod.getInstance());
    }

    void playAnimation() {
        mIcon.playAnimation();
    }

    void setAnimation(@RawRes int resId) {
        mIcon.setAnimation(resId);
    }
}
