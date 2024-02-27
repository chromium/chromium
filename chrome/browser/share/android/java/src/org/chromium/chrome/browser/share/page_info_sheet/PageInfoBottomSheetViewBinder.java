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

            view.mLoadingIndicator.setVisibility(
                    currentState == PageInfoState.INITIALIZING ? View.VISIBLE : View.GONE);
            view.mContentText.setVisibility(
                    currentState != PageInfoState.INITIALIZING ? View.VISIBLE : View.GONE);
            view.mAcceptButton.setEnabled(currentState == PageInfoState.SUCCESS);
            view.mAcceptButton.setVisibility(
                    currentState != PageInfoState.ERROR ? View.VISIBLE : View.GONE);
            view.mRefreshButton.setVisibility(
                    currentState == PageInfoState.SUCCESS ? View.VISIBLE : View.GONE);
        } else if (PageInfoBottomSheetProperties.CONTENT_TEXT == propertyKey) {
            view.mContentText.setText(model.get(PageInfoBottomSheetProperties.CONTENT_TEXT));
        } else if (PageInfoBottomSheetProperties.ON_ACCEPT_CLICKED == propertyKey) {
            view.mAcceptButton.setOnClickListener(
                    model.get(PageInfoBottomSheetProperties.ON_ACCEPT_CLICKED));
        } else if (PageInfoBottomSheetProperties.ON_CANCEL_CLICKED == propertyKey) {
            view.mCancelButton.setOnClickListener(
                    model.get(PageInfoBottomSheetProperties.ON_CANCEL_CLICKED));
        } else if (PageInfoBottomSheetProperties.ON_REFRESH_CLICKED == propertyKey) {
            view.mRefreshButton.setOnClickListener(
                    model.get(PageInfoBottomSheetProperties.ON_REFRESH_CLICKED));
        }
    }
}
