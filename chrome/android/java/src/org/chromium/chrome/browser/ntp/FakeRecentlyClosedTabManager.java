// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/**
 * A fake implementation of {@link RecentlyClosedTabManager} for testing purposes.
 */
public class FakeRecentlyClosedTabManager implements RecentlyClosedTabManager {
    @Nullable
    private Runnable mTabsUpdatedRunnable;
    private List<RecentlyClosedTab> mTabs = new ArrayList<>();

    @Override
    public void setTabsUpdatedRunnable(@Nullable Runnable runnable) {
        mTabsUpdatedRunnable = runnable;
    }

    @Override
    public List<RecentlyClosedTab> getRecentlyClosedTabs(int maxTabCount) {
        List<RecentlyClosedTab> tabs = new ArrayList<>();
        for (int i = 0; i < maxTabCount && i < mTabs.size(); i++) {
            tabs.add(mTabs.get(i));
        }
        return tabs;
    }

    @Override
    public boolean openRecentlyClosedTab(
            Tab tab, RecentlyClosedTab recentTab, int windowOpenDisposition) {
        return false;
    }

    @Override
    public void openRecentlyClosedTab() {}

    @Override
    public void clearRecentlyClosedTabs() {
        mTabs.clear();
        if (mTabsUpdatedRunnable != null) mTabsUpdatedRunnable.run();
    }

    @Override
    public void destroy() {}

    public void setRecentlyClosedTabs(List<RecentlyClosedTab> tabs) {
        mTabs = new ArrayList<>(tabs);
        if (mTabsUpdatedRunnable != null) mTabsUpdatedRunnable.run();
    }
}
