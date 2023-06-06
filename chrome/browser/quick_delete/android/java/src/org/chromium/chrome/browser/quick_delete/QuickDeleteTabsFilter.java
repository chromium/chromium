// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/**
 * A class responsible for providing logic around filtered tabs.
 */
class QuickDeleteTabsFilter {
    private static final long FIFTEEN_MINUTES_IN_MS = 15 * 60 * 1000;
    private final TabModel mTabModel;

    /**
     * This is needed because the code relies on {@link System#currentTimeMillis()} which is not
     * possible to mock.
     */
    private @Nullable Long mCurrentTimeForTesting;

    /**
     * @param tabModel A regular {@link TabModel} which is used to observe the tab related changes.
     */
    QuickDeleteTabsFilter(@NonNull TabModel tabModel) {
        assert !tabModel.isIncognito() : "Incognito tab model is not supported.";
        mTabModel = tabModel;
    }

    /**
     * A method to close tabs which were either created or had a navigation committed, in the
     * last 15 minutes.
     */
    void closeTabsFilteredForQuickDelete() {
        List<Tab> mTabs = getListOfTabsToBeClosed();
        mTabModel.closeMultipleTabs(mTabs, /*canUndo=*/false);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    List<Tab> getListOfTabsToBeClosed() {
        List<Tab> mTabList = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); ++i) {
            Tab tab = mTabModel.getTabAt(i);
            if (tab == null || tab.isCustomTab()) continue;

            final long recentNavigationTime =
                    CriticalPersistedTabData.from(tab).getLastNavigationCommittedTimestampMillis();
            final long currentTime = getCurrentTime();

            if (recentNavigationTime > currentTime - FIFTEEN_MINUTES_IN_MS) {
                mTabList.add(tab);
            }
        }
        return mTabList;
    }

    @VisibleForTesting
    void setCurrentTimeForTesting(long currentTime) {
        mCurrentTimeForTesting = currentTime;
    }

    private long getCurrentTime() {
        if (mCurrentTimeForTesting != null) {
            return mCurrentTimeForTesting;
        } else {
            return System.currentTimeMillis();
        }
    }
}
