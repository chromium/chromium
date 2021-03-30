// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.page_info;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.components.page_info.PageInfoAction;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;

/**
 * Controller to manage the history elements of PageInfo. Including the row view, subpage, and
 * forget site button.
 */
public class PageInfoHistoryController implements PageInfoSubpageController {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final String mTitle;
    private final String mPageUrl;

    public PageInfoHistoryController(PageInfoMainController mainController, PageInfoRowView rowView,
            PageInfoControllerDelegate delegate, String pageUrl) {
        mMainController = mainController;
        mRowView = rowView;
        mDelegate = delegate;
        mPageUrl = pageUrl;
        mTitle = mRowView.getContext().getResources().getString(R.string.page_info_history_title);

        setupHistoryRow();
    }

    private void launchSubpage() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_HISTORY_OPENED);
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        return mTitle;
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        // TODO(crbug.com/1173154): Should launch the history subpage.
        return new View(mRowView.getContext());
    }

    @Override
    public void onSubpageRemoved() {}

    private void setupHistoryRow() {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = getRowTitle();
        rowParams.visible = mDelegate.isSiteSettingsAvailable();
        rowParams.iconResId = R.drawable.ic_history_googblue_24dp;
        rowParams.clickCallback = this::launchSubpage;

        mRowView.setParams(rowParams);
    }

    private String getRowTitle() {
        // TODO(crbug.com/1173154): This should return string about how long since last visit.
        return mTitle;
    }

    @Override
    public void clearData() {}
}
