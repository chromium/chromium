// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

/** A tab model observer for managing bulk closures. */
public class HistoricalTabModelObserver implements TabModelObserver {
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final HistoricalTabSaver mHistoricalTabSaver;

    /**
     * @param tabGroupModelFilter The tab model filter to observe tab closures in.
     */
    public HistoricalTabModelObserver(TabGroupModelFilter tabGroupModelFilter) {
        this(tabGroupModelFilter, new HistoricalTabSaverImpl(tabGroupModelFilter.getTabModel()));
    }

    @VisibleForTesting
    public HistoricalTabModelObserver(
            TabGroupModelFilter tabGroupModelFilter, HistoricalTabSaver historicalTabSaver) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mHistoricalTabSaver = historicalTabSaver;

        tabGroupModelFilter.addObserver(this);
    }

    /** Removes observers. */
    public void destroy() {
        mTabGroupModelFilter.removeObserver(this);
        mHistoricalTabSaver.destroy();
    }

    /**
     * Adds a secondary {@link TabModel} supplier to check if a deleted tab should be added to
     * recent tabs.
     */
    public void addSecodaryTabModelSupplier(Supplier<TabModel> tabModelSupplier) {
        mHistoricalTabSaver.addSecodaryTabModelSupplier(tabModelSupplier);
    }

    /**
     * Removes a secondary {@link TabModel} supplier to check if a deleted tab should be added to
     * recent tabs.
     */
    public void removeSecodaryTabModelSupplier(Supplier<TabModel> tabModelSupplier) {
        mHistoricalTabSaver.removeSecodaryTabModelSupplier(tabModelSupplier);
    }

    @Override
    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
        if (tabs.isEmpty() || !canRestore) return;

        if (tabs.size() == 1) {
            Tab tab = tabs.get(0);
            if (!isTabGroupWithOneTab(tab)) {
                mHistoricalTabSaver.createHistoricalTab(tab);
                return;
            }
        }

        buildGroupsAndCreateClosure(tabs);
    }

    private void buildGroupsAndCreateClosure(List<Tab> tabs) {
        HashMap<Integer, HistoricalEntry> idToGroup = new HashMap<>();
        List<HistoricalEntry> entries = new ArrayList<>();

        Profile profile = mTabGroupModelFilter.getTabModel().getProfile();
        @Nullable
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);

        LazyOneshotSupplier<Set<Token>> tabGroupIdsInComprehensiveModel =
                mTabGroupModelFilter.getLazyAllTabGroupIds(
                        tabs, /* includePendingClosures= */ true);
        for (Tab tab : tabs) {
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId != null) {
                // If an entire tab group is closing we do not want to save them if:
                //
                // 1) They are being hidden so they remain in the TabGroupSyncService. As they can
                // be recalled from the tab groups pane.
                // 2) They are for a collaboration that is fully deleted. I.e. the user left the
                // group or deleted it as the owner.
                //
                // A tab group is fully closing if the comprehensive model no longer contains the
                // tab group ID. If the entry is still present in the comprehensive model, the tab
                // group is not fully closing and we can proceed with the process of saving an
                // event to recent tabs.
                if ((mTabGroupModelFilter.isTabGroupHiding(tabGroupId)
                                || isCollaborationTabGroup(tabGroupSyncService, tabGroupId))
                        && !tabGroupIdsInComprehensiveModel.get().contains(tabGroupId)) {
                    continue;
                }
            }

            // {@link TabGroupModelFilter} removes tabs from its data model as soon as they are
            // pending closure so it cannot be directly relied upon for group structure. Instead
            // rely on the underlying root ID in the tab's persisted data which is used to restore
            // groups across an pending closure cancellation (undo). The root ID is the group ID
            // unless the tab is ungrouped in which case the root ID is the tab's ID.
            int rootId = tab.getRootId();
            if (idToGroup.containsKey(rootId)) {
                idToGroup.get(rootId).getTabs().add(tab);
                continue;
            }
            // null title for default title is handled in HistoricalTabSaver.
            String title = mTabGroupModelFilter.getTabGroupTitle(rootId);
            // Give a tab group the first color in the color list as a placeholder.
            @TabGroupColorId int color = mTabGroupModelFilter.getTabGroupColorWithFallback(rootId);
            List<Tab> groupTabs = new ArrayList<>();
            groupTabs.add(tab);
            HistoricalEntry historicalGroup =
                    new HistoricalEntry(rootId, tabGroupId, title, color, groupTabs);
            entries.add(historicalGroup);
            idToGroup.put(rootId, historicalGroup);
        }

        // If only a subset of tabs in the tab group are closing tabs should be saved individually
        // so that a duplicate of the tab group isn't created on restore.
        // TODO(crbug/327166316): Wire up tab group IDs when saving in native so that the tabs
        // can be restored into their prior group if it still exists.
        List<HistoricalEntry> groupAdjustedEntries = new ArrayList<>();
        for (HistoricalEntry entry : entries) {
            if (shouldSaveSeparateTabs(entry)) {
                for (Tab tab : entry.getTabs()) {
                    groupAdjustedEntries.add(new HistoricalEntry(tab));
                }
            } else {
                groupAdjustedEntries.add(entry);
            }
        }

        mHistoricalTabSaver.createHistoricalBulkClosure(groupAdjustedEntries);
    }

    private boolean shouldSaveSeparateTabs(HistoricalEntry entry) {
        if (entry.getRootId() == Tab.INVALID_TAB_ID) return false;

        int rootId = entry.getRootId();
        boolean groupExists = mTabGroupModelFilter.tabGroupExistsForRootId(rootId);
        if (groupExists
                && entry.getTabs().size()
                        != mTabGroupModelFilter.getRelatedTabCountForRootId(rootId)) {
            // Case: Group information not lost yet (non-undoable closure). Rely on whether all the
            // tabs in the group are closing.
            return true;
        } else if (!groupExists) {
            // Case: Group information already lost (undoable closure). Rely on whether any unclosed
            // tabs share a root ID with the closing group.
            TabList comprehensiveModel = mTabGroupModelFilter.getTabModel().getComprehensiveModel();
            for (int i = 0; i < comprehensiveModel.getCount(); i++) {
                if (rootId == comprehensiveModel.getTabAt(i).getRootId()) return true;
            }
        }
        return false;
    }

    private boolean isTabGroupWithOneTab(Tab tab) {
        int rootId = tab.getRootId();
        if (!mTabGroupModelFilter.tabGroupExistsForRootId(rootId)) {
            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) return false;
            // Case: Group information already lost (undoable closure). Rely on whether the tab
            // still has a tab group ID.
            TabList comprehensiveModel = mTabGroupModelFilter.getTabModel().getComprehensiveModel();
            for (int i = 0; i < comprehensiveModel.getCount(); i++) {
                if (tabGroupId.equals(comprehensiveModel.getTabAt(i).getTabGroupId())) return false;
            }
            return true;
        } else {
            // Case: Group information not lost yet (non-undoable closure). Rely on whether the tab
            // is the only tab in its tab group.
            return mTabGroupModelFilter.isTabInTabGroup(tab)
                    && mTabGroupModelFilter.getRelatedTabCountForRootId(rootId) == 1;
        }
    }

    private boolean isCollaborationTabGroup(
            @Nullable TabGroupSyncService tabGroupSyncService, Token tabGroupId) {
        if (tabGroupSyncService == null) return false;

        SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(new LocalTabGroupId(tabGroupId));
        if (savedTabGroup == null) return false;

        return !TextUtils.isEmpty(savedTabGroup.collaborationId);
    }
}
