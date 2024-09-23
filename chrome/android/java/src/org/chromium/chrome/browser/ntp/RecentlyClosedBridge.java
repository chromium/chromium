// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.List;

/** This class allows Java code to get and clear the list of recently closed entries. */
@JNINamespace("recent_tabs")
public class RecentlyClosedBridge implements RecentlyClosedTabManager {
    private long mNativeBridge;
    private final TabModelSelector mTabModelSelector;

    @Nullable private Runnable mEntriesUpdatedRunnable;

    @CalledByNative
    private static void addTabToEntries(List<RecentlyClosedEntry> entries, RecentlyClosedTab tab) {
        entries.add(tab);
    }

    @CalledByNative
    private static void addGroupToEntries(
            List<RecentlyClosedEntry> entries,
            int id,
            long groupTimestamp,
            @JniType("std::u16string") String groupTitle,
            @TabGroupColorId int groupColor,
            @JniType("std::vector") List<RecentlyClosedTab> tabs) {
        RecentlyClosedGroup group =
                new RecentlyClosedGroup(id, groupTimestamp, groupTitle, groupColor);
        group.getTabs().addAll(tabs);
        entries.add(group);
    }

    @CalledByNative
    private static void addBulkEventToEntries(
            List<RecentlyClosedEntry> entries,
            int id,
            long eventTimestamp,
            @JniType("std::vector<std::optional<base::Token>>") Token[] tabGroupIds,
            @JniType("std::vector<const std::u16string*>") String[] groupTitles,
            @JniType("std::vector") List<RecentlyClosedTab> tabs) {
        RecentlyClosedBulkEvent event = new RecentlyClosedBulkEvent(id, eventTimestamp);

        assert tabGroupIds.length == groupTitles.length;
        for (int i = 0; i < tabGroupIds.length; i++) {
            event.getTabGroupIdToTitleMap().put(tabGroupIds[i], groupTitles[i]);
        }

        event.getTabs().addAll(tabs);
        entries.add(event);
    }

    @CalledByNative
    private void restoreTabGroup(
            TabModel tabModel,
            @JniType("std::string") String savedTabGroupId,
            @JniType("std::u16string") String title,
            int color,
            @JniType("std::vector") int[] tabIds) {
        if (tabIds.length == 0) return;

        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        TabModelFilter filter =
                mTabModelSelector
                        .getTabModelFilterProvider()
                        .getTabModelFilter(tabModel.isIncognito());
        assert filter instanceof TabGroupModelFilter;
        TabGroupModelFilter groupFilter = (TabGroupModelFilter) filter;

        int rootId = tabIds[0];

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            groupFilter.setTabGroupColor(rootId, color);
        }

        // TODO(b/336589861): Use savedTabGroupId to reassociate this tab group with a sync entity.

        if (tabIds.length == 1) {
            groupFilter.createSingleTabGroup(tabIds[0], false);
        } else {
            for (int id : tabIds) {
                if (id == rootId) continue;

                groupFilter.mergeTabsToGroup(id, rootId);
            }
        }

        if (title == null || title.isEmpty()) return;

        groupFilter.setTabGroupTitle(rootId, title);
    }

    /**
     * Initializes this class with the given profile.
     *
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
        boolean received =
                RecentlyClosedBridgeJni.get()
                        .getRecentlyClosedEntries(mNativeBridge, entries, maxEntryCount);
        return received ? entries : null;
    }

    @Override
    public boolean openRecentlyClosedTab(
            TabModel tabModel, RecentlyClosedTab recentTab, int windowOpenDisposition) {
        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        return RecentlyClosedBridgeJni.get()
                .openRecentlyClosedTab(
                        mNativeBridge, tabModel, recentTab.getSessionId(), windowOpenDisposition);
    }

    @Override
    public boolean openRecentlyClosedEntry(TabModel tabModel, RecentlyClosedEntry recentEntry) {
        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        return RecentlyClosedBridgeJni.get()
                .openRecentlyClosedEntry(mNativeBridge, tabModel, recentEntry.getSessionId());
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

    /** This method will be called every time the list of recently closed tabs is updated. */
    @CalledByNative
    private void onUpdated() {
        if (mEntriesUpdatedRunnable != null) mEntriesUpdatedRunnable.run();
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(RecentlyClosedBridge caller, @JniType("Profile*") Profile profile);

        void destroy(long nativeRecentlyClosedTabsBridge);

        boolean getRecentlyClosedEntries(
                long nativeRecentlyClosedTabsBridge,
                List<RecentlyClosedEntry> entries,
                int maxEntryCount);

        boolean openRecentlyClosedTab(
                long nativeRecentlyClosedTabsBridge,
                TabModel tabModel,
                int tabSessionId,
                int windowOpenDisposition);

        boolean openRecentlyClosedEntry(
                long nativeRecentlyClosedTabsBridge, TabModel tabModel, int sessionId);

        boolean openMostRecentlyClosedEntry(long nativeRecentlyClosedTabsBridge, TabModel tabModel);

        void clearRecentlyClosedEntries(long nativeRecentlyClosedTabsBridge);
    }
}
