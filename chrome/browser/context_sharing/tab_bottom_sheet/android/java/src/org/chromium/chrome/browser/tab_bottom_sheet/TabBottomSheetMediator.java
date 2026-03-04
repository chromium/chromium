// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for tab bottom sheet */
@NullMarked
public class TabBottomSheetMediator {
    private final PropertyModel mModel;
    private final CoBrowseViews mCoBrowseViews;

    public TabBottomSheetMediator(PropertyModel model, CoBrowseViews coBrowseViews) {
        mModel = model;
        mCoBrowseViews = coBrowseViews;
    }

    void onSheetOffsetChanged(float totalHeight) {
        // Set the ThinWebView to its full height when the sheet is opened. This stops it
        // from resizing when we change the height of the webUi container.
        if (mModel.get(TabBottomSheetProperties.THIN_WEB_VIEW_HEIGHT) == null) {
            mModel.set(
                    TabBottomSheetProperties.THIN_WEB_VIEW_HEIGHT,
                    mCoBrowseViews.getThinWebViewHeight());
        }

        float fuseboxHeight = mCoBrowseViews.getFuseboxHeight();
        float toolbarHeight = mCoBrowseViews.getToolbarHeight();

        int webUiHeight = (int) (totalHeight - fuseboxHeight - toolbarHeight);
        if (webUiHeight < 0) {
            webUiHeight = 0;
        }
        mModel.set(TabBottomSheetProperties.WEB_UI_CONTAINER_HEIGHT, webUiHeight);

        float thinWebViewHeight = mCoBrowseViews.getThinWebViewHeight();
        int thinWebViewInsetBottom = (int) (thinWebViewHeight - totalHeight);
        if (thinWebViewInsetBottom < 0) {
            thinWebViewInsetBottom = 0;
        }
        mModel.set(TabBottomSheetProperties.THIN_WEB_VIEW_INSET_BOTTOM, thinWebViewInsetBottom);
    }
}
