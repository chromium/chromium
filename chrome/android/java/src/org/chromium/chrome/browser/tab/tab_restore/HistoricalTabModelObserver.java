// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;
import android.util.ArrayMap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshotFactory;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Supplier;

/**
 * A {@link TabModelObserver} that saves closed tabs and tab groups to {@link HistoricalTabSaver}
 * and, when {@link ChromeFeatureList#sUndoableTabClosureRework} is enabled, captures an {@link
 * UndoClosureSnapshot} to notify the registered undo callback for undoable closures.
 */
@NullMarked
public class HistoricalTabModelObserver implements TabModelObserver {
    private final TabModel mTabModel;
    private final HistoricalTabSaver mHistoricalTabSaver;
    private @Nullable Callback<UndoClosureSnapshot> mUndoClosureCallback;

    /**
     * @param tabModel The tab model to observe tab closures in.
     */
    public HistoricalTabModelObserver(TabModel tabModel) {
        this(tabModel, new HistoricalTabSaverImpl(tabModel));
    }

    @VisibleForTesting
    public HistoricalTabModelObserver(TabModel tabModel, HistoricalTabSaver historicalTabSaver) {
        mTabModel = tabModel;
        mHistoricalTabSaver = historicalTabSaver;

        tabModel.addObserver(this);
    }

    /** Removes observers. */
    public void destroy() {
        mUndoClosureCallback = null;
        mTabModel.removeObserver(this);
        mHistoricalTabSaver.destroy();
    }

    /**
     * Sets the callback invoked with an {@link UndoClosureSnapshot} when an undoable closure is
     * saved to {@link HistoricalTabSaver}.
     */
    public void setUndoClosureCallback(
            @Nullable Callback<UndoClosureSnapshot> undoClosureCallback) {
        mUndoClosureCallback = undoClosureCallback;
    }

    /**
     * Adds a secondary {@link TabModel} supplier to check if a deleted tab should be added to
     * recent tabs.
     */
    public void addSecondaryTabModelSupplier(Supplier<@Nullable TabModel> tabModelSupplier) {
        mHistoricalTabSaver.addSecondaryTabModelSupplier(tabModelSupplier);
    }

    /**
     * Removes a secondary {@link TabModel} supplier to check if a deleted tab should be added to
     * recent tabs.
     */
    public void removeSecondaryTabModelSupplier(Supplier<@Nullable TabModel> tabModelSupplier) {
        mHistoricalTabSaver.removeSecondaryTabModelSupplier(tabModelSupplier);
    }

    @Override
    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
        onFinishingMultipleTabClosure(
                tabs,
                canRestore,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null,
                /* isAllTabs= */ false,
                /* isUndoable= */ false);
    }

    @Override
    public void onFinishingMultipleTabClosure(
            List<Tab> tabs,
            boolean canRestore,
            @TabClosingSource int closingSource,
            @Nullable Runnable undoRunnable,
            boolean isAllTabs,
            boolean isUndoable) {
        if (tabs.isEmpty() || !canRestore) return;

        @TabRestoreEntryId int entryId = saveToTabRestoreService(tabs);
        if (!isUndoable
                || mUndoClosureCallback == null
                || !ChromeFeatureList.sUndoableTabClosureRework.isEnabled()
                || mTabModel.isIncognitoBranded()) {
            return;
        }
        if (entryId == HistoricalTabSaver.INVALID_TAB_RESTORE_ENTRY_ID
                && !hasHidingTabGroup(tabs)) {
            return;
        }

        // For closures finalized at close time (isUndoable == true),
        // TabCollectionTabModelImpl#closeTabsInternal invokes onFinishingMultipleTabClosure
        // before removeTabsAndSelectNext removes the closing tabs or closes detached tab groups,
        // so the tabs, selection state, and group metadata are still live in mTabModel. Capturing
        // savedTabGroupSyncId via TabGroupSyncService#getGroup(LocalTabGroupId) also relies on
        // HistoricalTabModelObserver running before
        // TabGroupSyncLocalObserver#onFinishingMultipleTabClosure() calls
        // removeLocalTabGroupMapping() in the same observer pass.
        Profile profile = mTabModel.getProfile();
        assert profile != null;
        boolean isTabGroupSyncEnabled = TabGroupSyncFeatures.isTabGroupSyncEnabled(profile);
        @Nullable TabGroupSyncService tabGroupSyncService =
                isTabGroupSyncEnabled ? TabGroupSyncServiceFactory.getForProfile(profile) : null;

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        mTabModel,
                        tabs,
                        isAllTabs,
                        isTabGroupSyncEnabled,
                        tabGroupSyncService,
                        closingSource,
                        entryId,
                        undoRunnable);
        if (entryId == HistoricalTabSaver.INVALID_TAB_RESTORE_ENTRY_ID
                && !snapshot.hasReopenableHiddenGroups()) {
            return;
        }
        mUndoClosureCallback.onResult(snapshot);
    }

    private @TabRestoreEntryId int saveToTabRestoreService(List<Tab> tabs) {
        if (tabs.size() == 1) {
            Tab tab = tabs.get(0);
            if (!isTabGroupWithOneTab(tab)) {
                return mHistoricalTabSaver.createHistoricalTab(tab);
            }
        }

        return buildGroupsAndCreateClosure(tabs);
    }

    private @TabRestoreEntryId int buildGroupsAndCreateClosure(List<Tab> tabs) {
        Map<Token, HistoricalEntry> tabGroupIdToGroup = new ArrayMap<>();
        List<HistoricalEntry> entries = new ArrayList<>();

        Profile profile = assumeNonNull(mTabModel.getProfile());
        @Nullable TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(profile);

        LazyOneshotSupplier<Set<Token>> tabGroupIdsInComprehensiveModel =
                mTabModel.getLazyAllTabGroupIds(tabs, /* includePendingClosures= */ true);
        for (Tab tab : tabs) {
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) {
                entries.add(new HistoricalEntry(tab));
                continue;
            }

            // If an entire tab group is closing we do not want to save it if:
            //
            // 1) The tab group is being hidden so it remains in the TabGroupSyncService.
            // 2) It is for a collaboration that is fully deleted. I.e. the user left the group
            //    or deleted it as the owner.
            //
            // A tab group is fully closing if the comprehensive model no longer contains the tab
            // group ID. If the entry is still present in the comprehensive model, the tab group is
            // not fully closing and we can proceed with the process of saving an event to recent
            // tabs.
            if ((mTabModel.isTabGroupHiding(tabGroupId)
                            || isCollaborationTabGroup(tabGroupSyncService, tabGroupId))
                    && !assumeNonNull(tabGroupIdsInComprehensiveModel.get()).contains(tabGroupId)) {
                continue;
            }

            // {@link TabModel} removes tabs from its data model as soon as they are
            // pending closure so it cannot be directly relied upon for group structure. Instead
            // rely on the underlying tab group ID in the tab's persisted data which is used to
            // restore groups across an pending closure cancellation (undo).
            @Nullable HistoricalEntry tabGroupEntry = tabGroupIdToGroup.get(tabGroupId);
            if (tabGroupEntry != null) {
                tabGroupEntry.getTabs().add(tab);
                continue;
            }

            // A null title (default title) is handled in HistoricalTabSaver.
            String title = mTabModel.getTabGroupTitle(tabGroupId);
            // Give a tab group the first color in the color list as a placeholder.
            @TabGroupColorId int color = mTabModel.getTabGroupColorWithFallback(tabGroupId);

            List<Tab> groupTabs = new ArrayList<>();
            groupTabs.add(tab);
            HistoricalEntry historicalGroup =
                    new HistoricalEntry(
                            tabGroupId, title, color, groupTabs, /* savedTabGroupSyncId= */ null);
            entries.add(historicalGroup);
            tabGroupIdToGroup.put(tabGroupId, historicalGroup);
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

        return mHistoricalTabSaver.createHistoricalBulkClosure(groupAdjustedEntries);
    }

    private boolean shouldSaveSeparateTabs(HistoricalEntry entry) {
        @Nullable Token tabGroupId = entry.getTabGroupId();
        if (tabGroupId == null) return false;

        boolean groupExists = mTabModel.containsTabGroup(tabGroupId);
        if (groupExists) {
            List<Tab> tabsInGroup = mTabModel.getTabsInGroup(tabGroupId);
            if (tabsInGroup.size() != entry.getTabs().size()
                    || !entry.getTabs().containsAll(tabsInGroup)) {
                // Case: Group information not lost yet (non-undoable closure). Rely on whether all
                // the
                // tabs in the group are closing.
                return true;
            }
        } else {
            // Case: Group information already lost (undoable closure). Rely on whether any unclosed
            // tabs share a tab group id with the closing group.
            if (isTabGroupIdInComprehensiveModel(tabGroupId)) {
                return true;
            }
        }
        return false;
    }

    private boolean isTabGroupWithOneTab(Tab tab) {
        @Nullable Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) return false;

        if (mTabModel.containsTabGroup(tabGroupId)) {
            // Case: Group information not lost yet (non-undoable closure). Rely on whether the tab
            // is the only tab in its tab group.
            List<Tab> tabs = mTabModel.getTabsInGroup(tabGroupId);
            return tabs.size() == 1 && tabs.contains(tab);
        } else {
            // Case: Group information already lost (undoable closure). Rely on whether the tab
            // still has a tab group ID.
            return !isTabGroupIdInComprehensiveModel(tabGroupId);
        }
    }

    private boolean isTabGroupIdInComprehensiveModel(Token tabGroupId) {
        TabList comprehensiveModel = mTabModel.getComprehensiveModel();
        for (Tab tabInComprehensiveModel : comprehensiveModel) {
            if (tabGroupId.equals(tabInComprehensiveModel.getTabGroupId())) {
                return true;
            }
        }
        return false;
    }

    private boolean isCollaborationTabGroup(
            @Nullable TabGroupSyncService tabGroupSyncService, Token tabGroupId) {
        if (tabGroupSyncService == null) return false;

        SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(new LocalTabGroupId(tabGroupId));
        if (savedTabGroup == null) return false;

        return !TextUtils.isEmpty(savedTabGroup.collaborationId);
    }

    private boolean hasHidingTabGroup(List<Tab> tabs) {
        for (Tab tab : tabs) {
            @Nullable Token groupId = tab.getTabGroupId();
            if (groupId != null && mTabModel.isTabGroupHiding(groupId)) {
                return true;
            }
        }
        return false;
    }
}
