// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.text.TextUtils;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetCoordinator.PageInfoContents;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetProperties.PageInfoState;
import org.chromium.chrome.browser.share.page_info_sheet.PageSummaryMetrics.PageSummarySheetEvents;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** PageInfoBottomSheetMediator is responsible for retrieving, and attaching page info. */
class PageInfoBottomSheetMediator extends EmptyBottomSheetObserver {

    private final PropertyModel mModel;
    private final PageInfoBottomSheetCoordinator.Delegate mPageInfoDelegate;
    private final PageInfoBottomSheetContent mPageInfoBottomSheetContent;
    private final BottomSheetController mBottomSheetController;
    private final Callback<PageInfoContents> mOnContentsChangedCallback = this::onContentsChanged;

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
                                PageInfoBottomSheetProperties.ON_LEARN_MORE_CLICKED,
                                this::onLearnMoreClicked)
                        .with(
                                PageInfoBottomSheetProperties.ON_POSITIVE_FEEDBACK_CLICKED,
                                this::onPositiveFeedbackClicked)
                        .with(
                                PageInfoBottomSheetProperties.ON_NEGATIVE_FEEDBACK_CLICKED,
                                this::onNegativeFeedbackClicked)
                        .with(PageInfoBottomSheetProperties.STATE, PageInfoState.INITIALIZING)
                        .build();

        mBottomSheetController.addObserver(this);
        mPageInfoDelegate.getContentSupplier().addObserver(mOnContentsChangedCallback);
    }

    private void onLearnMoreClicked(View view) {
        mPageInfoDelegate.onLearnMore();
    }

    private void onNegativeFeedbackClicked(View view) {
        mPageInfoDelegate.onNegativeFeedback();
    }

    private void onPositiveFeedbackClicked(View view) {
        mPageInfoDelegate.onPositiveFeedback();
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
                    StateChangeReason.TAP_SCRIM -> {
                dismissSheet();
            }
        }
    }

    private void onAcceptClicked(View view) {
        mPageInfoDelegate.onAccept();
    }

    private void onCancelClicked(View view) {
        dismissSheet();
    }

    private void dismissSheet() {
        recordStateOnDismiss();
        mPageInfoDelegate.onCancel();
    }

    private void recordStateOnDismiss() {
        @PageInfoState int state = mModel.get(PageInfoBottomSheetProperties.STATE);
        @PageSummarySheetEvents
        int stateOnDismiss = PageSummarySheetEvents.CLOSE_SHEET_WHILE_INITIALIZING;
        switch (state) {
            case PageInfoState.ERROR:
                stateOnDismiss = PageSummarySheetEvents.CLOSE_SHEET_ON_ERROR;
                break;
            case PageInfoState.LOADING:
                stateOnDismiss = PageSummarySheetEvents.CLOSE_SHEET_WHILE_LOADING;
                break;
            case PageInfoState.SUCCESS:
                stateOnDismiss = PageSummarySheetEvents.CLOSE_SHEET_AFTER_SUCCESS;
                break;
        }

        PageSummaryMetrics.recordSummarySheetEvent(stateOnDismiss);
    }

    @Override
    public void onSheetStateChanged(int newState, int reason) {
        // If we update the sheet contents while its animation is running it won't update its height
        // to the updated text size. Request expansion again to fix this.
        if (newState == SheetState.FULL
                && mBottomSheetController.getCurrentSheetContent() == mPageInfoBottomSheetContent
                && mPageInfoDelegate.getContentSupplier().hasValue()
                && mBottomSheetController.getCurrentOffset()
                        != mPageInfoBottomSheetContent.getContentView().getHeight()) {
            mBottomSheetController.expandSheet();
        }
    }

    void destroySheet() {
        mBottomSheetController.hideContent(mPageInfoBottomSheetContent, true);
        mPageInfoDelegate.getContentSupplier().removeObserver(mOnContentsChangedCallback);
        mBottomSheetController.removeObserver(this);
    }
}
