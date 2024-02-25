// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/** A fake implementation of {@link RecentlyClosedTabManager} for testing purposes. */
public class FakeRecentlyClosedTabManager implements RecentlyClosedTabManager {
    @Nullable private Runnable mEntriesUpdatedRunnable;
    private List<RecentlyClosedEntry> mTabs = new ArrayList<>();

    public FakeRecentlyClosedTabManager() {
        mEntriesUpdatedRunnable = null;
    }

    @Override
    public void setEntriesUpdatedRunnable(@Nullable Runnable runnable) {
        mEntriesUpdatedRunnable = runnable;
    }

    @Override
    public List<RecentlyClosedEntry> getRecentlyClosedEntries(int maxEntryCount) {
        List<RecentlyClosedEntry> entries = new ArrayList<>();
        for (int i = 0; i < maxEntryCount && i < mTabs.size(); i++) {
            entries.add(mTabs.get(i));
        }
        return entries;
    }

    @Override
    public boolean openRecentlyClosedTab(
            TabModel tabModel, RecentlyClosedTab recentTab, int windowOpenDisposition) {
        return false;
    }

    @Override
    public boolean openRecentlyClosedEntry(TabModel tabModel, RecentlyClosedEntry recentEntry) {
        return false;
    }

    @Override
    public void openMostRecentlyClosedEntry(TabModel tabModel) {}

    @Override
    public void clearRecentlyClosedEntries() {
        mTabs.clear();
        if (mEntriesUpdatedRunnable != null) mEntriesUpdatedRunnable.run();
    }

    @Override
    public void destroy() {}

    public void setRecentlyClosedEntries(List<RecentlyClosedEntry> tabs) {
        mTabs = new ArrayList<>(tabs);
        if (mEntriesUpdatedRunnable != null) mEntriesUpdatedRunnable.run();
    }
}
