// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.page_info;

import android.content.res.Resources;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.HistoryContentManager;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.components.browser_ui.util.date.StringUtils;
import org.chromium.components.page_info.PageInfoAction;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;

import java.util.Date;

/**
 * Class for controlling the page info history section.
 */
public class PageInfoHistoryController
        implements PageInfoSubpageController, HistoryContentManager.Observer {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final String mTitle;
    private final String mHost;
    private HistoryContentManager mContentManager;
    private long mLastVisitedTimestamp;

    public PageInfoHistoryController(PageInfoMainController mainController, PageInfoRowView rowView,
            PageInfoControllerDelegate delegate, String host) {
        mMainController = mainController;
        mRowView = rowView;
        mDelegate = delegate;
        mTitle = mRowView.getContext().getResources().getString(R.string.page_info_history_title);
        mHost = host;

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
        assert !mDelegate.isIncognito();
        mContentManager = new HistoryContentManager(mMainController.getActivity(), this,
                /* isSeparateActivity */ false,
                /* isIncognito */ false, /* shouldShowPrivacyDisclaimers */ true,
                /* shouldShowClearData */ false, mHost,
                /* selectionDelegate */ null, /* tabCreatorManager */ null,
                /* tabSupplier */ null);
        mContentManager.initialize();
        return mContentManager.getRecyclerView();
    }

    @Override
    public void onSubpageRemoved() {
        if (mContentManager != null) {
            mContentManager.onDestroyed();
            mContentManager = null;
        }
    }

    private void setupHistoryRow() {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = getRowTitle();
        rowParams.visible = rowParams.title != null && mDelegate.isSiteSettingsAvailable()
                && !mDelegate.isIncognito();
        rowParams.iconResId = R.drawable.ic_history_googblue_24dp;
        rowParams.clickCallback = this::launchSubpage;

        mRowView.setParams(rowParams);
    }

    private String getRowTitle() {
        if (mLastVisitedTimestamp == 0) {
            return mTitle;
        }
        // TODO(crbug.com/1173154): Set last visit timestamp based on history query.
        long difference = 0;
        Resources resources = mRowView.getContext().getResources();
        if (difference == 0) {
            return resources.getString(R.string.page_info_history_last_visit_today);
        } else if (difference == DateUtils.DAY_IN_MILLIS) {
            return resources.getString(R.string.page_info_history_last_visit_yesterday);
        } else if (difference > DateUtils.DAY_IN_MILLIS
                && difference <= DateUtils.DAY_IN_MILLIS * 7) {
            return resources.getString(R.string.page_info_history_last_visit_days,
                    (int) (difference / DateUtils.DAY_IN_MILLIS));
        } else {
            return resources.getString(R.string.page_info_history_last_visit_date,
                    StringUtils.dateToHeaderString(new Date(mLastVisitedTimestamp)));
        }
    }

    @Override
    public void clearData() {
        // TODO(crbug.com/1173154): Add functionality for clear history for this site.
        return;
    }

    // HistoryContentManager.Observer
    @Override
    public void onScrolledCallback(boolean loadedMore) {}

    // HistoryContentManager.Observer
    @Override
    public void onItemClicked(HistoryItem item) {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_HISTORY_ENTRY_CLICKED);
        return;
    }

    // HistoryContentManager.Observer
    @Override
    public void onItemRemoved(HistoryItem item) {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_HISTORY_ENTRY_REMOVED);
        return;
    }

    // HistoryContentManager.Observer
    @Override
    public void onClearBrowsingDataClicked() {
        // TODO(crbug.com/1173154): Add functionality for "clear history" button click and
        // change the name of the current clear browsing data button.
        return;
    }

    // HistoryContentManager.Observer
    @Override
    public void onPrivacyDisclaimerHasChanged() {}

    // HistoryContentManager.Observer
    @Override
    public void onUserAccountStateChanged() {}
}
