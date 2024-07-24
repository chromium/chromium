// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Manages observers that monitor for updates to tab group visual aspects such as colors and titles.
 */
public class TabGroupVisualDataManager {
    private static final int DELETE_DATA_GROUP_SIZE_THRESHOLD = 1;

    private final TabModelSelector mTabModelSelector;
    private TabModelObserver mTabModelObserver;
    private TabGroupModelFilterObserver mFilterObserver;

    public TabGroupVisualDataManager(@NonNull TabModelSelector tabModelSelector) {
        assert tabModelSelector.isTabStateInitialized();
        mTabModelSelector = tabModelSelector;

        TabModelFilterProvider tabModelFilterProvider =
                mTabModelSelector.getTabModelFilterProvider();

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        if (tabs.isEmpty()) return;

                        TabGroupModelFilter filter = filterFromTab(tabs.get(0));
                        LazyOneshotSupplier<Set<Integer>> remainingRootIds =
                                filter.getLazyAllRootIdsInComprehensiveModel(tabs);
                        Set<Integer> processedRootIds = new HashSet<>();
                        for (Tab tab : tabs) {
                            int rootId = tab.getRootId();
                            boolean wasAdded = processedRootIds.add(rootId);
                            if (!wasAdded) continue;

                            // If any related tab still exist keep the data as size 1 groups are
                            // valid.
                            if (remainingRootIds.get().contains(rootId)) continue;

                            Runnable deleteTask =
                                    () -> {
                                        filter.deleteTabGroupTitle(rootId);
                                        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                                            filter.deleteTabGroupColor(rootId);
                                        }
                                        if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
                                            filter.deleteTabGroupCollapsed(rootId);
                                        }
                                    };
                            if (filter.isTabGroupHiding(tab.getTabGroupId())) {
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
                    public void willMergeTabToGroup(Tab movedTab, int newRootId) {
                        TabGroupModelFilter filter = filterFromTab(movedTab);
                        String sourceGroupTitle = filter.getTabGroupTitle(movedTab.getRootId());
                        String targetGroupTitle = filter.getTabGroupTitle(newRootId);
                        // If the target group has no title but the source group has a title,
                        // handover the stored title to the group after merge.
                        if (sourceGroupTitle != null && targetGroupTitle == null) {
                            filter.setTabGroupTitle(newRootId, sourceGroupTitle);
                        }

                        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                            int sourceGroupColor = filter.getTabGroupColor(movedTab.getRootId());
                            int targetGroupColor = filter.getTabGroupColor(newRootId);
                            // If the target group has no color but the source group has a color,
                            // handover the stored color to the group after merge.
                            if (sourceGroupColor != TabGroupColorUtils.INVALID_COLOR_ID
                                    && targetGroupColor == TabGroupColorUtils.INVALID_COLOR_ID) {
                                filter.setTabGroupColor(newRootId, sourceGroupColor);
                            } else if (sourceGroupColor == TabGroupColorUtils.INVALID_COLOR_ID
                                    && targetGroupColor == TabGroupColorUtils.INVALID_COLOR_ID) {
                                filter.setTabGroupColor(
                                        newRootId,
                                        TabGroupColorUtils.getNextSuggestedColorId(filter));
                            }
                        }
                    }

                    @Override
                    public void willMoveTabOutOfGroup(Tab movedTab, int newRootId) {
                        TabGroupModelFilter filter = filterFromTab(movedTab);
                        int rootId = movedTab.getRootId();
                        String title = filter.getTabGroupTitle(rootId);

                        // If the group size is 2, i.e. the group becomes a single tab after
                        // ungroup, delete the stored visual data. When tab groups of size 1 are
                        // supported this behavior is no longer valid.
                        boolean shouldDeleteVisualData =
                                filter.getRelatedTabCountForRootId(rootId)
                                        <= DELETE_DATA_GROUP_SIZE_THRESHOLD;
                        if (shouldDeleteVisualData) {
                            if (title != null) {
                                filter.deleteTabGroupTitle(rootId);
                            }
                            if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                                filter.deleteTabGroupColor(rootId);
                            }
                            if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
                                filter.deleteTabGroupCollapsed(rootId);
                            }
                        }
                    }

                    @Override
                    public void didChangeGroupRootId(int oldRootId, int newRootId) {
                        TabGroupModelFilter filter =
                                filterFromTab(mTabModelSelector.getTabById(newRootId));
                        moveTabGroupMetadata(filter, oldRootId, newRootId);
                    }
                };

        tabModelFilterProvider.addTabModelFilterObserver(mTabModelObserver);

        ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(false))
                .addTabGroupObserver(mFilterObserver);
        ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(true))
                .addTabGroupObserver(mFilterObserver);
    }

    private TabGroupModelFilter filterFromTab(Tab tab) {
        return (TabGroupModelFilter)
                mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(tab.isIncognito());
    }

    /** Overwrites the tab group metadata at the new id with the data from the old id. */
    public static void moveTabGroupMetadata(
            TabGroupModelFilter filter, int oldRootId, int newRootId) {
        String title = filter.getTabGroupTitle(oldRootId);
        if (title != null) {
            filter.setTabGroupTitle(newRootId, title);
            filter.deleteTabGroupTitle(oldRootId);
        }
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            int colorId = filter.getTabGroupColor(oldRootId);
            if (colorId != TabGroupColorUtils.INVALID_COLOR_ID) {
                filter.setTabGroupColor(newRootId, colorId);
                filter.deleteTabGroupColor(oldRootId);
            }
        }
        if (ChromeFeatureList.sTabStripGroupCollapse.isEnabled()) {
            if (filter.getTabGroupCollapsed(oldRootId)) {
                filter.setTabGroupCollapsed(newRootId, true);
                filter.deleteTabGroupCollapsed(oldRootId);
            }
        }
    }

    /** Destroy any members that need clean up. */
    public void destroy() {
        TabModelFilterProvider tabModelFilterProvider =
                mTabModelSelector.getTabModelFilterProvider();

        if (mTabModelObserver != null) {
            tabModelFilterProvider.removeTabModelFilterObserver(mTabModelObserver);
            mTabModelObserver = null;
        }

        if (mFilterObserver != null) {
            ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(false))
                    .removeTabGroupObserver(mFilterObserver);
            ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(true))
                    .removeTabGroupObserver(mFilterObserver);
            mFilterObserver = null;
        }
    }
}
