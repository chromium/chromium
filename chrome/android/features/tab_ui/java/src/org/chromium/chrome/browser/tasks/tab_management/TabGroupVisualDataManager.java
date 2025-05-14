// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Manages observers that monitor for updates to tab group visual aspects such as colors and titles.
 */
@NullMarked
public class TabGroupVisualDataManager {
    private static final int DELETE_DATA_GROUP_SIZE_THRESHOLD = 1;

    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabGroupModelFilterObserver mFilterObserver;

    public TabGroupVisualDataManager(TabModelSelector tabModelSelector) {
        assert tabModelSelector.isTabStateInitialized();
        mTabModelSelector = tabModelSelector;

        TabGroupModelFilterProvider tabGroupModelFilterProvider =
                mTabModelSelector.getTabGroupModelFilterProvider();

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        if (tabs.isEmpty()) return;

                        TabGroupModelFilter filter = filterFromTab(tabs.get(0));
                        LazyOneshotSupplier<Set<Token>> remainingTabGroupIds =
                                filter.getLazyAllTabGroupIds(
                                        tabs, /* includePendingClosures= */ true);
                        Set<Token> processedTabGroupIds = new HashSet<>();
                        for (Tab tab : tabs) {
                            @Nullable Token tabGroupId = tab.getTabGroupId();
                            if (tabGroupId == null) continue;

                            boolean wasAdded = processedTabGroupIds.add(tabGroupId);
                            if (!wasAdded) continue;

                            // If any related tab still exist keep the data as size 1 groups are
                            // valid.
                            if (assumeNonNull(remainingTabGroupIds.get()).contains(tabGroupId)) {
                                continue;
                            }

                            int rootId = tab.getRootId();
                            Runnable deleteTask =
                                    () -> {
                                        filter.deleteTabGroupTitle(rootId);
                                        filter.deleteTabGroupColor(rootId);
                                        filter.deleteTabGroupCollapsed(rootId);
                                    };
                            if (filter.isTabGroupHiding(tabGroupId)) {
                                // Post this work because if the closure is non-undoable, but the
                                // tab group is hiding we don't want sync to pick up this deletion
                                // and we should post so all the observers are notified before we do
                                // the deletion.
                                PostTask.postTask(TaskTraits.UI_DEFAULT, deleteTask);
                            } else {
                                deleteTask.run();
                            }
                        }
                    }
                };

        mFilterObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMergeTabToGroup(
                            Tab movedTab, int newRootId, @Nullable Token tabGroupId) {
                        TabGroupModelFilter filter = filterFromTab(movedTab);
                        int rootId = movedTab.getRootId();
                        String sourceGroupTitle = filter.getTabGroupTitle(rootId);
                        String targetGroupTitle = filter.getTabGroupTitle(newRootId);
                        // If the target group has no title but the source group has a title,
                        // handover the stored title to the group after merge.
                        if (sourceGroupTitle != null && targetGroupTitle == null) {
                            filter.setTabGroupTitle(newRootId, sourceGroupTitle);
                        }

                        int sourceGroupColor = filter.getTabGroupColor(rootId);
                        int targetGroupColor = filter.getTabGroupColor(newRootId);
                        // If the target group has no color but the source group has a color,
                        // handover the stored color to the group after merge.
                        if (sourceGroupColor != TabGroupColorUtils.INVALID_COLOR_ID
                                && targetGroupColor == TabGroupColorUtils.INVALID_COLOR_ID) {
                            filter.setTabGroupColor(newRootId, sourceGroupColor);
                        } else if (sourceGroupColor == TabGroupColorUtils.INVALID_COLOR_ID
                                && targetGroupColor == TabGroupColorUtils.INVALID_COLOR_ID) {
                            filter.setTabGroupColor(
                                    newRootId, TabGroupColorUtils.getNextSuggestedColorId(filter));
                        }
                    }

                    @Override
                    public void willMoveTabOutOfGroup(
                            Tab movedTab, @Nullable Token destinationTabGroupId) {
                        TabGroupModelFilter filter = filterFromTab(movedTab);

                        // If the group will become empty (0 tabs) delete the title.
                        boolean shouldDeleteVisualData =
                                filter.getTabCountForGroup(movedTab.getTabGroupId())
                                        <= DELETE_DATA_GROUP_SIZE_THRESHOLD;
                        if (shouldDeleteVisualData) {
                            int rootId = movedTab.getRootId();
                            @Nullable String title = filter.getTabGroupTitle(rootId);
                            if (title != null) {
                                filter.deleteTabGroupTitle(rootId);
                            }

                            filter.deleteTabGroupColor(rootId);
                            filter.deleteTabGroupCollapsed(rootId);
                        }
                    }

                    @Override
                    public void didChangeGroupRootId(int oldRootId, int newRootId) {
                        TabGroupModelFilter filter =
                                filterFromTab(
                                        assumeNonNull(mTabModelSelector.getTabById(newRootId)));
                        moveTabGroupMetadata(filter, oldRootId, newRootId);
                    }
                };

        tabGroupModelFilterProvider.addTabGroupModelFilterObserver(mTabModelObserver);

        assumeNonNull(tabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .addTabGroupObserver(mFilterObserver);
        assumeNonNull(tabGroupModelFilterProvider.getTabGroupModelFilter(true))
                .addTabGroupObserver(mFilterObserver);
    }

    private TabGroupModelFilter filterFromTab(Tab tab) {
        return assumeNonNull(
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(tab.isIncognito()));
    }

    /** Overwrites the tab group metadata at the new id with the data from the old id. */
    public static void moveTabGroupMetadata(
            TabGroupModelFilter filter, int oldRootId, int newRootId) {
        String title = filter.getTabGroupTitle(oldRootId);
        if (title != null) {
            filter.setTabGroupTitle(newRootId, title);
            filter.deleteTabGroupTitle(oldRootId);
        }

        int colorId = filter.getTabGroupColor(oldRootId);
        if (colorId != TabGroupColorUtils.INVALID_COLOR_ID) {
            filter.setTabGroupColor(newRootId, colorId);
            filter.deleteTabGroupColor(oldRootId);
        }
        if (filter.getTabGroupCollapsed(oldRootId)) {
            filter.setTabGroupCollapsed(newRootId, true);
            filter.deleteTabGroupCollapsed(oldRootId);
        }
    }

    /** Destroy any members that need clean up. */
    public void destroy() {
        TabGroupModelFilterProvider tabGroupModelFilterProvider =
                mTabModelSelector.getTabGroupModelFilterProvider();
        tabGroupModelFilterProvider.removeTabGroupModelFilterObserver(mTabModelObserver);
        assumeNonNull(tabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .removeTabGroupObserver(mFilterObserver);
        assumeNonNull(tabGroupModelFilterProvider.getTabGroupModelFilter(true))
                .removeTabGroupObserver(mFilterObserver);
    }
}
