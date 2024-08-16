// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

import java.util.ArrayList;
import java.util.List;

/** A class responsible for providing logic around filtered tabs. */
class QuickDeleteTabsFilter {
    static final long FIFTEEN_MINUTES_IN_MS = 15 * 60 * 1000;
    static final long ONE_HOUR_IN_MS = FIFTEEN_MINUTES_IN_MS * 4;
    static final long ONE_DAY_IN_MS = ONE_HOUR_IN_MS * 24;
    static final long ONE_WEEK_IN_MS = ONE_DAY_IN_MS * 7;
    static final long FOUR_WEEKS_IN_MS = ONE_WEEK_IN_MS * 4;

    private final TabGroupModelFilter mTabGroupModelFilter;

    /**
     * List of tabs that are filtered for deletion. This should get updated every time the time
     * period changes and again when the deletion is confirmed.
     */
    private @Nullable List<Tab> mTabs;

    /**
     * This is needed because the code relies on {@link System#currentTimeMillis()} which is not
     * possible to mock.
     */
    private @Nullable Long mCurrentTimeForTesting;

    /**
     * @param tabModel A regular {@link TabGroupModelFilter} which is used to observe the tab
     *     related changes.
     */
    QuickDeleteTabsFilter(@NonNull TabGroupModelFilter tabGroupModelFilter) {
        assert !tabGroupModelFilter.isIncognito() : "Incognito tab model is not supported.";
        mTabGroupModelFilter = tabGroupModelFilter;
    }

    private List<Tab> getListOfAllTabsToBeClosed() {
        List<Tab> mTabList = new ArrayList<>();
        TabModel tabModel = mTabGroupModelFilter.getTabModel();
        for (int i = 0; i < tabModel.getCount(); ++i) {
            Tab tab = tabModel.getTabAt(i);
            if (tab == null || tab.isCustomTab()) continue;
            mTabList.add(tab);
        }
        return mTabList;
    }

    private long getCurrentTime() {
        if (mCurrentTimeForTesting != null) {
            return mCurrentTimeForTesting;
        } else {
            return System.currentTimeMillis();
        }
    }

    void setCurrentTimeForTesting(long currentTime) {
        mCurrentTimeForTesting = currentTime;
    }

    static long getTimePeriodToMilliseconds(@TimePeriod int timePeriod) {
        switch (timePeriod) {
            case TimePeriod.LAST_15_MINUTES:
                return FIFTEEN_MINUTES_IN_MS;
            case TimePeriod.LAST_HOUR:
                return ONE_HOUR_IN_MS;
            case TimePeriod.LAST_DAY:
                return ONE_DAY_IN_MS;
            case TimePeriod.LAST_WEEK:
                return ONE_WEEK_IN_MS;
            case TimePeriod.FOUR_WEEKS:
                return FOUR_WEEKS_IN_MS;
            default:
                throw new IllegalStateException("Unexpected value: " + timePeriod);
        }
    }

    /** Closes list of tabs currently filtered for deletion. */
    void closeTabsFilteredForQuickDelete() {
        assert mTabs != null;
        mTabGroupModelFilter.closeTabs(
                TabClosureParams.closeTabs(mTabs)
                        .allowUndo(false)
                        .saveToTabRestoreService(false)
                        .build());
    }

    /** Return list of tabs currently filtered for deletion. */
    List<Tab> getListOfTabsFilteredToBeClosed() {
        assert mTabs != null;
        return mTabs;
    }

    /**
     * Prepares a list of tabs which were either created or had a navigation committed within the
     * time period.
     */
    // TODO(crbug.com/40255099): Re-use CBD implementation of tab filtering & closure instead of
    // doing it here.
    void prepareListOfTabsToBeClosed(@TimePeriod int timePeriod) {
        if (TimePeriod.ALL_TIME == timePeriod) {
            mTabs = getListOfAllTabsToBeClosed();
            return;
        }

        List<Tab> mTabList = new ArrayList<>();
        TabModel tabModel = mTabGroupModelFilter.getTabModel();
        for (int i = 0; i < tabModel.getCount(); ++i) {
            Tab tab = tabModel.getTabAt(i);
            if (tab == null || tab.isCustomTab()) continue;

            final long recentNavigationTime = tab.getLastNavigationCommittedTimestampMillis();
            final long currentTime = getCurrentTime();

            if (recentNavigationTime > currentTime - getTimePeriodToMilliseconds(timePeriod)) {
                mTabList.add(tab);
            }
        }

        mTabs = mTabList;
    }
}
