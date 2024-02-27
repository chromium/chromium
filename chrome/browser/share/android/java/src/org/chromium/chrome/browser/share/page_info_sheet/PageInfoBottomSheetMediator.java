// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.text.TextUtils;
import android.view.View;

import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetCoordinator.PageInfoContents;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetProperties.PageInfoState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PageInfoBottomSheetMediator is responsible for retrieving, refreshing and attaching page info.
 */
class PageInfoBottomSheetMediator extends EmptyBottomSheetObserver {

    private final PropertyModel mModel;
    private final PageInfoBottomSheetCoordinator.Delegate mPageInfoDelegate;
    private final PageInfoBottomSheetContent mPageInfoBottomSheetContent;
    private final BottomSheetController mBottomSheetController;

    public PropertyModel getModel() {
        return mModel;
    }

    public PageInfoBottomSheetMediator(
            PageInfoBottomSheetCoordinator.Delegate pageInfoDelegate,
            PageInfoBottomSheetContent pageInfoBottomSheetContent,
            BottomSheetController bottomSheetController) {
        mPageInfoDelegate = pageInfoDelegate;
        mPageInfoBottomSheetContent = pageInfoBottomSheetContent;
        mBottomSheetController = bottomSheetController;
        mModel =
                PageInfoBottomSheetProperties.defaultModelBuilder()
                        .with(
                                PageInfoBottomSheetProperties.ON_ACCEPT_CLICKED,
                                this::onAcceptClicked)
                        .with(
                                PageInfoBottomSheetProperties.ON_CANCEL_CLICKED,
                                this::onCancelClicked)
                        .with(
                                PageInfoBottomSheetProperties.ON_REFRESH_CLICKED,
                                this::onRefreshClicked)
                        .with(PageInfoBottomSheetProperties.STATE, PageInfoState.INITIALIZING)
                        .build();

        mPageInfoDelegate.getContentSupplier().addObserver(this::onContentsChanged);
    }

    private void onContentsChanged(PageInfoContents contents) {
        if (contents == null) {
            mModel.set(PageInfoBottomSheetProperties.STATE, PageInfoState.INITIALIZING);
        } else if (!TextUtils.isEmpty(contents.errorMessage)) {
            mModel.set(PageInfoBottomSheetProperties.STATE, PageInfoState.ERROR);
            mModel.set(PageInfoBottomSheetProperties.CONTENT_TEXT, contents.errorMessage);
        } else if (!TextUtils.isEmpty(contents.resultContents)) {
            mModel.set(PageInfoBottomSheetProperties.CONTENT_TEXT, contents.resultContents);
            if (contents.isLoading) {
                mModel.set(PageInfoBottomSheetProperties.STATE, PageInfoState.LOADING);
            } else {
                mModel.set(PageInfoBottomSheetProperties.STATE, PageInfoState.SUCCESS);
            }
        }
    }

    boolean requestShowContent() {
        return mBottomSheetController.requestShowContent(mPageInfoBottomSheetContent, true);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        switch (reason) {
            case StateChangeReason.BACK_PRESS,
                    StateChangeReason.SWIPE,
                    StateChangeReason.TAP_SCRIM -> destroySheet();
        }
    }

    private void onAcceptClicked(View view) {
        mPageInfoDelegate.onAccept();
        mBottomSheetController.hideContent(mPageInfoBottomSheetContent, true);
    }

    private void onCancelClicked(View view) {
        mPageInfoDelegate.onCancel();
        mBottomSheetController.hideContent(mPageInfoBottomSheetContent, true);
    }

    private void onRefreshClicked(View view) {
        mPageInfoDelegate.onRefresh();
    }

    @Override
    public void onSheetOpened(int reason) {}

    void destroySheet() {
        mPageInfoDelegate.onCancel();
    }
}
