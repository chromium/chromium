// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.view.View;

import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetProperties.PageInfoState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

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
        }
    }
}
