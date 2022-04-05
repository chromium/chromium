// Copyright 2013 The Chromium Authors. All rights reserved.
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
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * This class allows Java code to get and clear the list of recently closed tabs.
 */
@JNINamespace("recent_tabs")
public class RecentlyClosedBridge implements RecentlyClosedTabManager {
    private long mNativeBridge;

    @Nullable
    private Runnable mTabsUpdatedRunnable;

    // TODO(crbug/1307345): Remove in favor of generic entries.
    @CalledByNative
    private static void pushTab(List<RecentlyClosedTab> tabs, int id, long timestamp, String title,
            GURL url, String groupId) {
        RecentlyClosedTab tab = new RecentlyClosedTab(id, timestamp, title, url, groupId);
        tabs.add(tab);
    }

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

    /**
     * Initializes this class with the given profile.
     * @param profile The Profile whose recently closed tabs will be queried.
     */
    public RecentlyClosedBridge(Profile profile) {
        mNativeBridge = RecentlyClosedBridgeJni.get().init(RecentlyClosedBridge.this, profile);
    }

    @Override
    public void destroy() {
        assert mNativeBridge != 0;
        RecentlyClosedBridgeJni.get().destroy(mNativeBridge);
        mNativeBridge = 0;
        mTabsUpdatedRunnable = null;
    }

    @Override
    public void setTabsUpdatedRunnable(@Nullable Runnable runnable) {
        mTabsUpdatedRunnable = runnable;
    }

    @Override
    public List<RecentlyClosedTab> getRecentlyClosedTabs(int maxTabCount) {
        List<RecentlyClosedTab> tabs = new ArrayList<RecentlyClosedTab>();
        boolean received = RecentlyClosedBridgeJni.get().getRecentlyClosedTabs(
                mNativeBridge, tabs, maxTabCount);
        return received ? tabs : null;
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
        return RecentlyClosedBridgeJni.get().openRecentlyClosedTab(
                mNativeBridge, tabModel, recentTab.getSessionId(), windowOpenDisposition);
    }

    @Override
    public void openMostRecentlyClosedTab(TabModel tabModel) {
        RecentlyClosedBridgeJni.get().openMostRecentlyClosedTab(mNativeBridge, tabModel);
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
        if (mTabsUpdatedRunnable != null) mTabsUpdatedRunnable.run();
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(RecentlyClosedBridge caller, Profile profile);
        void destroy(long nativeRecentlyClosedTabsBridge);
        boolean getRecentlyClosedTabs(
                long nativeRecentlyClosedTabsBridge, List<RecentlyClosedTab> tabs, int maxTabCount);
        boolean getRecentlyClosedEntries(long nativeRecentlyClosedTabsBridge,
                List<RecentlyClosedEntry> entries, int maxEntryCount);
        boolean openRecentlyClosedTab(long nativeRecentlyClosedTabsBridge, TabModel tabModel,
                int recentTabId, int windowOpenDisposition);
        boolean openMostRecentlyClosedTab(long nativeRecentlyClosedTabsBridge, TabModel tabModel);
        void clearRecentlyClosedEntries(long nativeRecentlyClosedTabsBridge);
    }
}
