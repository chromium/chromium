// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * This class allows Java code to get and clear the list of recently closed entries.
 */
@JNINamespace("recent_tabs")
public class RecentlyClosedBridge implements RecentlyClosedTabManager {
    private long mNativeBridge;
    private final TabModelSelector mTabModelSelector;

    @Nullable
    private Runnable mEntriesUpdatedRunnable;

    private static void addTabs(List<RecentlyClosedTab> tabs, int[] tabIds, long[] tabTimestamps,
            String[] tabTitles, GURL[] tabUrls, String[] tabGroupIds) {
        assert tabIds.length == tabTimestamps.length;
        assert tabIds.length == tabTitles.length;
        assert tabIds.length == tabUrls.length;
        assert tabIds.length == tabGroupIds.length;
        for (int i = 0; i < tabIds.length; i++) {
            tabs.add(new RecentlyClosedTab(
                    tabIds[i], tabTimestamps[i], tabTitles[i], tabUrls[i], tabGroupIds[i]));
        }
    }

    @CalledByNative
    private static void addTabToEntries(List<RecentlyClosedEntry> entries, int id, long timestamp,
            String title, GURL url, String groupId) {
        RecentlyClosedTab tab = new RecentlyClosedTab(id, timestamp, title, url, groupId);
        entries.add(tab);
    }

    @CalledByNative
    private static void addGroupToEntries(List<RecentlyClosedEntry> entries, int id,
            long groupTimestamp, String groupTitle, int[] tabIds, long[] tabTimestamps,
            String[] tabTitles, GURL[] tabUrls, String[] tabGroupIds) {
        RecentlyClosedGroup group = new RecentlyClosedGroup(id, groupTimestamp, groupTitle);

        addTabs(group.getTabs(), tabIds, tabTimestamps, tabTitles, tabUrls, tabGroupIds);

        entries.add(group);
    }

    @CalledByNative
    private static void addBulkEventToEntries(List<RecentlyClosedEntry> entries, int id,
            long eventTimestamp, String[] groupIds, String[] groupsTitles, int[] tabIds,
            long[] tabTimestamps, String[] tabTitles, GURL[] tabUrls, String[] tabGroupIds) {
        RecentlyClosedBulkEvent event = new RecentlyClosedBulkEvent(id, eventTimestamp);

        assert groupIds.length == groupsTitles.length;
        for (int i = 0; i < groupIds.length; i++) {
            event.getGroupIdToTitleMap().put(groupIds[i], groupsTitles[i]);
        }

        addTabs(event.getTabs(), tabIds, tabTimestamps, tabTitles, tabUrls, tabGroupIds);

        entries.add(event);
    }

    @CalledByNative
    private void restoreTabGroup(TabModel tabModel, int groupId, String title, int[] tabIds) {
        // Can't restore empty or size 1 group. Note groupId is a tab in the group.
        if (tabIds.length < 1) return;

        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        TabModelFilter filter = mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(
                tabModel.isIncognito());
        if (!(filter instanceof TabGroupModelFilter)) return;

        TabGroupModelFilter groupFilter = (TabGroupModelFilter) filter;
        for (int id : tabIds) {
            // This shouldn't happen, but as a precaution skip.
            if (id == groupId) continue;

            groupFilter.mergeTabsToGroup(id, groupId);
        }

        if (title == null || title.isEmpty()) return;

        TabGroupTitleUtils.storeTabGroupTitle(groupId, title);
    }

    /**
     * Initializes this class with the given profile.
     * @param profile The {@link Profile} whose recently closed tabs will be queried.
     * @param tabModelSelector The {@link TabModelSelector} to use to get {@link TabModelFilter}s.
     */
    public RecentlyClosedBridge(Profile profile, TabModelSelector tabModelSelector) {
        mNativeBridge = RecentlyClosedBridgeJni.get().init(RecentlyClosedBridge.this, profile);
        mTabModelSelector = tabModelSelector;
    }

    @Override
    public void destroy() {
        assert mNativeBridge != 0;
        RecentlyClosedBridgeJni.get().destroy(mNativeBridge);
        mNativeBridge = 0;
        mEntriesUpdatedRunnable = null;
    }

    @Override
    public void setEntriesUpdatedRunnable(@Nullable Runnable runnable) {
        mEntriesUpdatedRunnable = runnable;
    }

    @Override
    public List<RecentlyClosedEntry> getRecentlyClosedEntries(int maxEntryCount) {
        List<RecentlyClosedEntry> entries = new ArrayList<RecentlyClosedEntry>();
        boolean received = RecentlyClosedBridgeJni.get().getRecentlyClosedEntries(
                mNativeBridge, entries, maxEntryCount);
        return received ? entries : null;
    }

    @Override
    public boolean openRecentlyClosedTab(
            TabModel tabModel, RecentlyClosedTab recentTab, int windowOpenDisposition) {
        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        return RecentlyClosedBridgeJni.get().openRecentlyClosedTab(
                mNativeBridge, tabModel, recentTab.getSessionId(), windowOpenDisposition);
    }

    @Override
    public boolean openRecentlyClosedEntry(TabModel tabModel, RecentlyClosedEntry recentEntry) {
        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        return RecentlyClosedBridgeJni.get().openRecentlyClosedEntry(
                mNativeBridge, tabModel, recentEntry.getSessionId());
    }

    @Override
    public void openMostRecentlyClosedEntry(TabModel tabModel) {
        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        RecentlyClosedBridgeJni.get().openMostRecentlyClosedEntry(mNativeBridge, tabModel);
    }

    @Override
    public void clearRecentlyClosedEntries() {
        RecentlyClosedBridgeJni.get().clearRecentlyClosedEntries(mNativeBridge);
    }

    /**
     * This method will be called every time the list of recently closed tabs is updated.
     */
    @CalledByNative
    private void onUpdated() {
        if (mEntriesUpdatedRunnable != null) mEntriesUpdatedRunnable.run();
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(RecentlyClosedBridge caller, Profile profile);
        void destroy(long nativeRecentlyClosedTabsBridge);
        boolean getRecentlyClosedEntries(long nativeRecentlyClosedTabsBridge,
                List<RecentlyClosedEntry> entries, int maxEntryCount);
        boolean openRecentlyClosedTab(long nativeRecentlyClosedTabsBridge, TabModel tabModel,
                int tabSessionId, int windowOpenDisposition);
        boolean openRecentlyClosedEntry(
                long nativeRecentlyClosedTabsBridge, TabModel tabModel, int sessionId);
        boolean openMostRecentlyClosedEntry(long nativeRecentlyClosedTabsBridge, TabModel tabModel);
        void clearRecentlyClosedEntries(long nativeRecentlyClosedTabsBridge);
    }
}
