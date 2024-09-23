// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.text.method.LinkMovementMethod;
import android.view.View;

import androidx.constraintlayout.widget.ConstraintSet;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetProperties.PageInfoState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Class responsible for binding the model and the view. */
class PageInfoBottomSheetViewBinder {
    static void bind(PropertyModel model, PageInfoBottomSheetView view, PropertyKey propertyKey) {

        if (PageInfoBottomSheetProperties.STATE == propertyKey) {
            @PageInfoState int currentState = model.get(PageInfoBottomSheetProperties.STATE);

            int visibleWhenInitializing =
                    currentState == PageInfoState.INITIALIZING ? View.VISIBLE : View.GONE;
            int visibleWhenNotInitializing =
                    currentState != PageInfoState.INITIALIZING ? View.VISIBLE : View.GONE;
            int visibleWhenSuccessful =
                    currentState == PageInfoState.SUCCESS ? View.VISIBLE : View.GONE;

            ConstraintSet constraintSet = new ConstraintSet();
            constraintSet.clone(view);
            if (currentState == PageInfoState.INITIALIZING) {
                // On the initializing state constrain the contents container to the bottom of the
                // sheet.
                constraintSet.connect(
                        view.mContentsContainer.getId(),
                        ConstraintSet.BOTTOM,
                        ConstraintSet.PARENT_ID,
                        ConstraintSet.BOTTOM);
                constraintSet.applyTo(view);
                view.mContentsContainer.setBackgroundResource(
                        R.drawable.page_info_bottom_sheet_loading_background);
            } else {
                // On all other states constrain the contents container to the bottom of the
                // feedback area, leaving space for mAcceptButton and mCancelButton.
                constraintSet.connect(
                        view.mContentsContainer.getId(),
                        ConstraintSet.BOTTOM,
                        R.id.feedback_area_bottom_barrier,
                        ConstraintSet.BOTTOM);
                constraintSet.applyTo(view);
                view.mContentsContainer.setBackgroundResource(
                        R.drawable.page_info_bottom_sheet_content_background);
            }

            // Views only shown when initializing.
            view.mLoadingIndicator.setVisibility(visibleWhenInitializing);
            // Views hidden only when initializing.
            view.mContentText.setVisibility(visibleWhenNotInitializing);
            // Views shown only when successful.
            view.mAcceptButton.setVisibility(visibleWhenSuccessful);
            view.mCancelButton.setVisibility(visibleWhenSuccessful);
            view.mPositiveFeedbackButton.setVisibility(visibleWhenSuccessful);
            view.mNegativeFeedbackButton.setVisibility(visibleWhenSuccessful);
            view.mLearnMoreText.setVisibility(visibleWhenSuccessful);
            view.mFeedbackDivider.setVisibility(visibleWhenSuccessful);
        } else if (PageInfoBottomSheetProperties.CONTENT_TEXT == propertyKey) {
            view.mContentText.setText(model.get(PageInfoBottomSheetProperties.CONTENT_TEXT));
        } else if (PageInfoBottomSheetProperties.ON_ACCEPT_CLICKED == propertyKey) {
            view.mAcceptButton.setOnClickListener(
                    model.get(PageInfoBottomSheetProperties.ON_ACCEPT_CLICKED));
        } else if (PageInfoBottomSheetProperties.ON_CANCEL_CLICKED == propertyKey) {
            view.mCancelButton.setOnClickListener(
                    model.get(PageInfoBottomSheetProperties.ON_CANCEL_CLICKED));
            view.mBackButton.setOnClickListener(
                    model.get(PageInfoBottomSheetProperties.ON_CANCEL_CLICKED));
        } else if (PageInfoBottomSheetProperties.ON_POSITIVE_FEEDBACK_CLICKED == propertyKey) {
            view.mPositiveFeedbackButton.setOnClickListener(
                    model.get(PageInfoBottomSheetProperties.ON_POSITIVE_FEEDBACK_CLICKED));
        } else if (PageInfoBottomSheetProperties.ON_NEGATIVE_FEEDBACK_CLICKED == propertyKey) {
            view.mNegativeFeedbackButton.setOnClickListener(
                    model.get(PageInfoBottomSheetProperties.ON_NEGATIVE_FEEDBACK_CLICKED));
        } else if (PageInfoBottomSheetProperties.ON_LEARN_MORE_CLICKED == propertyKey) {
            Callback<View> onLearnMoreClickedCallback =
                    model.get(PageInfoBottomSheetProperties.ON_LEARN_MORE_CLICKED);
            NoUnderlineClickableSpan settingsLink =
                    new NoUnderlineClickableSpan(view.getContext(), onLearnMoreClickedCallback);

            view.mLearnMoreText.setText(
                    SpanApplier.applySpans(
                            view.getResources()
                                    .getString(R.string.share_with_summary_sheet_disclaimer),
                            new SpanApplier.SpanInfo("<link>", "</link>", settingsLink)));
            view.mLearnMoreText.setMovementMethod(LinkMovementMethod.getInstance());
        }
    }
}
