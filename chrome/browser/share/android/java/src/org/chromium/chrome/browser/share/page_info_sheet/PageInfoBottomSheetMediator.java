// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PageInfoBottomSheetMediator is responsible for retrieving, refreshing and attaching page info.
 */
class PageInfoBottomSheetMediator extends EmptyBottomSheetObserver {

    /** Refers to the result of trying to show the page info UI. */
    @IntDef({
        PageInfoUiResult.ACCEPTED,
        PageInfoUiResult.REJECTED,
        PageInfoUiResult.REFRESHED,
        PageInfoUiResult.FAILED
    })
    public @interface PageInfoUiResult {
        /** Page info was shown and user accepted. */
        int ACCEPTED = 0;

        /** Page info was shown and user rejected or dismissed. */
        int REJECTED = 1;

        /** Page info refresh was requested. */
        int REFRESHED = 2;

        /** Page info failed to fetch. */
        int FAILED = 4;
    }

    private PageInfoBottomSheet mPageInfoBottomSheet;
    private final BottomSheetController mBottomSheetController;
    private PropertyModel mModel;

    public PageInfoBottomSheetMediator(
            PageInfoBottomSheet pageInfoBottomSheet, BottomSheetController bottomSheetController) {
        mPageInfoBottomSheet = pageInfoBottomSheet;
        mBottomSheetController = bottomSheetController;
        mModel = PageInfoBottomSheetProperties.defaultModelBuilder().build();
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        switch (reason) {
            case StateChangeReason.BACK_PRESS:
                destroySheet(PageInfoUiResult.REJECTED);
                break;
            case StateChangeReason.SWIPE:
                destroySheet(PageInfoUiResult.REJECTED);
                break;
            case StateChangeReason.TAP_SCRIM:
                destroySheet(PageInfoUiResult.REJECTED);
                break;
            default:
                destroySheet(PageInfoUiResult.REJECTED);
                break;
        }
    }

    @Override
    public void onSheetOpened(int reason) {}

    private void destroySheet(@PageInfoUiResult int callbackResult) {}
}
