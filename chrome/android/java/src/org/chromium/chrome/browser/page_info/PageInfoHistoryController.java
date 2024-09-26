// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.res.Resources;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryContentManager;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider;
import org.chromium.chrome.browser.history.HistoryUmaRecorder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.browser_ui.util.date.StringUtils;
import org.chromium.components.page_info.PageInfoAction;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;

import java.util.Date;

/** Class for controlling the page info history section. */
public class PageInfoHistoryController
        implements PageInfoSubpageController, HistoryContentManager.Observer {
    public static final int HISTORY_ROW_ID = View.generateViewId();

    private static HistoryProvider sProviderForTests;

    /** Clock to use so we can mock time in tests. */
    public interface Clock {
        long currentTimeMillis();
    }

    private static Clock sClock = System::currentTimeMillis;

    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final Supplier<Tab> mTabSupplier;
    private final String mTitle;
    private final String mHost;
    private boolean mDataIsStale;
    private HistoryProvider mHistoryProvider;
    private HistoryContentManager mContentManager;
    private long mLastVisitedTimestamp;

    public PageInfoHistoryController(
            PageInfoMainController mainController,
            PageInfoRowView rowView,
            PageInfoControllerDelegate delegate,
            Supplier<Tab> tabSupplier) {
        mMainController = mainController;
        mRowView = rowView;
        mDelegate = delegate;
        mTitle = mRowView.getContext().getResources().getString(R.string.page_info_history_title);
        mHost = mainController.getURL().getHost();
        mTabSupplier = tabSupplier;

        updateLastVisit();
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
        Profile profile = (Profile) mDelegate.getBrowserContext();
        mContentManager =
                new HistoryContentManager(
                        mMainController.getActivity(),
                        this,
                        /* isSeparateActivity= */ false,
                        /* profile= */ profile,
                        /* shouldShowPrivacyDisclaimers= */ true,
                        /* shouldShowClearDataIfAvailable= */ false,
                        mHost,
                        /* selectionDelegate= */ null,
                        /* bottomSheetController= */ null,
                        mTabSupplier,
                        /* hideSoftKeyboard= */ null,
                        /* umaRecorder= */ new HistoryUmaRecorder(),
                        new BrowsingHistoryBridge(profile),
                        null,
                        /* launchedForApp= */ false,
                        /* showAppFilter= */ false);
        mContentManager.startLoadingItems();
        return mContentManager.getRecyclerView();
    }

    @Override
    public void onSubpageRemoved() {
        if (mContentManager != null) {
            mContentManager.onDestroyed();
            mContentManager = null;
        }
    }

    private void updateLastVisit() {
        mHistoryProvider =
                sProviderForTests != null
                        ? sProviderForTests
                        : new BrowsingHistoryBridge((Profile) mDelegate.getBrowserContext());
        mHistoryProvider.getLastVisitToHostBeforeRecentNavigations(
                mHost,
                (timestamp) -> {
                    mLastVisitedTimestamp = timestamp;
                    if (mHistoryProvider != null) {
                        mHistoryProvider.destroy();
                        mHistoryProvider = null;
                    }
                    setupHistoryRow();
                });
    }

    private void setupHistoryRow() {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = getRowTitle();
        rowParams.visible =
                rowParams.title != null
                        && mDelegate.isSiteSettingsAvailable()
                        && !mDelegate.isIncognito();
        rowParams.iconResId = R.drawable.ic_history_googblue_24dp;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;

        mRowView.setParams(rowParams);
    }

    private String getRowTitle() {
        if (mLastVisitedTimestamp == 0) {
            return null;
        }
        long today = CalendarUtils.getStartOfDay(sClock.currentTimeMillis()).getTime().getTime();
        long lastVisitedDay =
                CalendarUtils.getStartOfDay(mLastVisitedTimestamp).getTime().getTime();
        long difference = today - lastVisitedDay;
        Resources resources = mRowView.getContext().getResources();
        if (difference < 0) {
            return null;
        } else if (difference == 0) {
            return resources.getString(R.string.page_info_history_last_visit_today);
        } else if (difference == DateUtils.DAY_IN_MILLIS) {
            return resources.getString(R.string.page_info_history_last_visit_yesterday);
        } else if (difference > DateUtils.DAY_IN_MILLIS
                && difference <= DateUtils.DAY_IN_MILLIS * 7) {
            return resources.getString(
                    R.string.page_info_history_last_visit_days,
                    (int) (difference / DateUtils.DAY_IN_MILLIS));
        } else {
            return resources.getString(
                    R.string.page_info_history_last_visit_date,
                    StringUtils.dateToHeaderString(new Date(mLastVisitedTimestamp)));
        }
    }

    @Override
    public void clearData() {
        // TODO(crbug.com/40746014): Add functionality for clear history for this site.

    }

    @Override
    public void updateRowIfNeeded() {
        if (mDataIsStale) {
            updateLastVisit();
        }
        mDataIsStale = false;
    }

    // HistoryContentManager.Observer
    @Override
    public void onScrolledCallback(boolean loadedMore) {}

    // HistoryContentManager.Observer
    @Override
    public void onItemClicked(HistoryItem item) {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_HISTORY_ENTRY_CLICKED);
    }

    // HistoryContentManager.Observer
    @Override
    public void onItemRemoved(HistoryItem item) {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_HISTORY_ENTRY_REMOVED);
        mDataIsStale = true;
        if (mContentManager.getItemCount() == 0) {
            // Do the update right away if there are no entries left.
            mLastVisitedTimestamp = 0;
            setupHistoryRow();
            mMainController.exitSubpage();
        }
    }

    // HistoryContentManager.Observer
    @Override
    public void onClearBrowsingDataClicked() {
        // TODO(crbug.com/40746014): Add functionality for "clear history" button click and
        // change the name of the current clear browsing data button.

    }

    // HistoryContentManager.Observer
    @Override
    public void onOpenFullChromeHistoryClicked() {}

    // HistoryContentManager.Observer
    @Override
    public void onPrivacyDisclaimerHasChanged() {}

    // HistoryContentManager.Observer
    @Override
    public void onUserAccountStateChanged() {}

    // HistoryContentManager.Observer
    @Override
    public void onHistoryDeletedExternally() {}

    /** @param provider The {@link HistoryProvider} that is used in place of a real one. */
    public static void setProviderForTests(HistoryProvider provider) {
        sProviderForTests = provider;
        ResettersForTesting.register(() -> sProviderForTests = null);
    }

    static void setClockForTesting(Clock clock) {
        var oldValue = sClock;
        sClock = clock;
        ResettersForTesting.register(() -> sClock = oldValue);
    }
}
