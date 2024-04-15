// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;

/**
 * Manages observers that monitor for updates to tab group visual aspects such as colors and titles.
 */
public class TabGroupVisualDataManager {
    private static final int INVALID_COLOR_ID = -1;

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
                    public void tabClosureCommitted(Tab tab) {
                        TabGroupModelFilter filter = filterFromTab(tab);
                        int rootId = tab.getRootId();
                        Tab groupTab = filter.getGroupLastShownTab(rootId);
                        if (groupTab == null || !filter.isTabInTabGroup(groupTab)) {
                            TabGroupTitleUtils.deleteTabGroupTitle(rootId);

                            if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                                TabGroupColorUtils.deleteTabGroupColor(rootId);
                            }
                        }
                    }
                };

        mFilterObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMergeTabToGroup(Tab movedTab, int newRootId) {
                        TabGroupModelFilter filter = filterFromTab(movedTab);
                        String sourceGroupTitle =
                                TabGroupTitleUtils.getTabGroupTitle(movedTab.getRootId());
                        String targetGroupTitle = TabGroupTitleUtils.getTabGroupTitle(newRootId);
                        // If the target group has no title but the source group has a title,
                        // handover the stored title to the group after merge.
                        if (sourceGroupTitle != null && targetGroupTitle == null) {
                            filter.setTabGroupTitle(newRootId, sourceGroupTitle);
                        }

                        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                            int sourceGroupColor =
                                    TabGroupColorUtils.getTabGroupColor(movedTab.getRootId());
                            int targetGroupColor = TabGroupColorUtils.getTabGroupColor(newRootId);
                            // If the target group has no color but the source group has a color,
                            // handover the stored color to the group after merge.
                            if (sourceGroupColor != INVALID_COLOR_ID
                                    && targetGroupColor == INVALID_COLOR_ID) {
                                filter.setTabGroupColor(newRootId, sourceGroupColor);
                            }
                        }
                    }

                    @Override
                    public void willMoveTabOutOfGroup(Tab movedTab, int newRootId) {
                        int rootId = movedTab.getRootId();
                        String title = TabGroupTitleUtils.getTabGroupTitle(rootId);

                        // If the group size is 2, i.e. the group becomes a single tab after
                        // ungroup, delete the stored visual data. When tab groups of size 1 are
                        // supported this behavior is no longer valid.
                        TabGroupModelFilter filter = filterFromTab(movedTab);
                        int sizeThreshold =
                                ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled() ? 1 : 2;
                        boolean shouldDeleteVisualData =
                                filter.getRelatedTabCountForRootId(rootId) <= sizeThreshold;
                        if (shouldDeleteVisualData) {
                            if (title != null) {
                                TabGroupTitleUtils.deleteTabGroupTitle(rootId);
                            }

                            if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                                TabGroupColorUtils.deleteTabGroupColor(rootId);
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
        String title = TabGroupTitleUtils.getTabGroupTitle(oldRootId);
        if (title != null) {
            filter.setTabGroupTitle(newRootId, title);
            TabGroupTitleUtils.deleteTabGroupTitle(oldRootId);
        }

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            int colorId = TabGroupColorUtils.getTabGroupColor(oldRootId);
            if (colorId != INVALID_COLOR_ID) {
                filter.setTabGroupColor(newRootId, colorId);
                TabGroupColorUtils.deleteTabGroupColor(oldRootId);
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
