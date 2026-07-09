// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.util.Pair;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * {@link TabListMediator.TabListLayoutType#GROUPED} implementation of {@link
 * TabGroupObserverDelegate}.
 */
@NullMarked
class GroupedTabGroupObserverDelegate extends TabGroupObserverDelegate {
    private final @Nullable ThumbnailProvider mThumbnailProvider;

    GroupedTabGroupObserverDelegate(
            TabListMediator mediator,
            TabListModel modelList,
            @Nullable ThumbnailProvider thumbnailProvider) {
        super(mediator, modelList);
        mThumbnailProvider = thumbnailProvider;
    }

    @Override
    public void didMoveWithinGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
        if (mThumbnailProvider != null) {
            int indexInModel = mMediator.getIndexForTabIdWithRelatedTabs(movedTab.getId());
            if (indexInModel == TabModel.INVALID_TAB_INDEX) return;

            TabModel tabModel = mMediator.getCurrentTabModelChecked();
            Tab lastShownTab =
                    tabModel.getRepresentativeTabAt(tabModel.representativeIndexOf(movedTab));
            assumeNonNull(lastShownTab);
            PropertyModel model = mModelList.get(indexInModel).model;
            mMediator.updateThumbnailFetcher(model, lastShownTab.getId());
        }
    }

    @Override
    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab previousGroupTab = tabModel.getRepresentativeTabAt(prevFilterIndex);
        assumeNonNull(previousGroupTab);

        Token movedTabGroupId = movedTab.getTabGroupId();
        if (tabModel.getTabCountForGroup(movedTabGroupId) <= 1 && movedTab != previousGroupTab) {
            // Add a tab to the model if it represents a new card. This happens if
            // the tab is either not in a group or in a group by itself. We do this
            // first so that the indices for the filter and the model match when
            // doing the update afterwards. When moving a tab between groups, the
            // new tab being added to an existing group is handled in
            // didMergeTabToGroup().
            int filterIndex = tabModel.representativeIndexOf(movedTab);
            mMediator.addTabCardToModel(movedTab, mModelList.indexOfNthTabCard(filterIndex));
        } else if (movedTabGroupId != null
                && movedTabGroupId.equals(previousGroupTab.getTabGroupId())) {
            // Despite being ungrouped we are still in a tab group this could mean
            // the previous tab card this tab was associated with no longer contains
            // tabs. If we have the same tab group id as the previous group tab then
            // this was possibly the last tab in its group. Remove the tab card if
            // it exists.
            int previousIndex = mModelList.indexFromTabId(movedTab.getId());
            if (previousIndex != TabModel.INVALID_TAB_INDEX) {
                mModelList.removeAt(previousIndex);
                return;
            }
        }
        // Always update the previous group to clean up old state e.g. thumbnail,
        // title, etc.
        mMediator.updateTab(
                mModelList.indexOfNthTabCard(prevFilterIndex), previousGroupTab, true, false);
    }

    @Override
    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(movedTab.getId());
        Pair<Integer, Integer> positions =
                mModelList.getIndexesForMergeToGroup(
                        tabModel, movedTab, isDestinationTab, relatedTabs);
        int srcIndex = positions.second;
        int desIndex = positions.first;

        // If only the desIndex is valid then just update the destination index to
        // the last shown tab in its group.
        if (desIndex != TabModel.INVALID_TAB_INDEX && srcIndex == TabModel.INVALID_TAB_INDEX) {
            @TabId int desIndexTabId = mModelList.get(desIndex).model.get(TabProperties.TAB_ID);
            Tab desTab = tabModel.getTabById(desIndexTabId);
            assumeNonNull(desTab);
            Token desTabGroupId = desTab.getTabGroupId();
            Tab lastShownTab = desTab;
            if (desTabGroupId != null) {
                @TabId int lastShownTabId = tabModel.getGroupLastShownTabId(desTabGroupId);
                if (lastShownTabId != Tab.INVALID_TAB_ID) {
                    lastShownTab = tabModel.getTabById(lastShownTabId);
                }
            }
            assert lastShownTab != null;
            mMediator.updateTab(desIndex, lastShownTab, true, false);
            return;
        }

        if (!mModelList.isValidIndex(srcIndex) || !mModelList.isValidIndex(desIndex)) {
            return;
        }

        // We merged the source group to the destination group. Remove the source
        // group and update the destination group.
        mModelList.removeAt(srcIndex);
        desIndex = srcIndex > desIndex ? desIndex : mModelList.getTabIndexBefore(desIndex);
        Tab newSelectedTabInMergedGroup =
                tabModel.getRepresentativeTabAt(mModelList.getTabCardCountsBefore(desIndex));
        assumeNonNull(newSelectedTabInMergedGroup);
        if (newSelectedTabInMergedGroup != null) {
            mMediator.updateTab(desIndex, newSelectedTabInMergedGroup, true, false);
        }

        // TODO(crbug.com/434246302): These metrics are probably wrong as it looks
        // like they get emitted per-tab merged, rather than per-group merged.
        if (mMediator.getRelatedTabsForId(movedTab.getId()).size() == 2) {
            // When users use drop-to-merge to create a group.
            RecordUserAction.record("TabGroup.Created.DropToMerge");
        } else {
            RecordUserAction.record("TabGrid.Drag.DropToMerge");
        }
    }

    @Override
    public void didMoveTabGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
        List<Tab> relatedTabs = mMediator.getRelatedTabsForId(movedTab.getId());
        TabModel tabModel = mMediator.getCurrentTabModelChecked();
        Tab currentGroupSelectedTab = TabGroupUtils.getSelectedTabInGroupForTab(tabModel, movedTab);
        int curPosition = mModelList.indexFromTabId(currentGroupSelectedTab.getId());
        if (curPosition == TabModel.INVALID_TAB_INDEX) {
            // Sync TabListModel with updated TabModel.
            int indexToUpdate =
                    mModelList.indexOfNthTabCard(
                            tabModel.representativeIndexOf(tabModel.getTabAt(tabModelOldIndex)));
            mModelList.updateTabListModelIdForGroup(currentGroupSelectedTab, indexToUpdate);
            curPosition = mModelList.indexFromTabId(currentGroupSelectedTab.getId());
        }
        if (!mModelList.isValidIndex(curPosition)) return;

        // TODO(crbug.com/526117174): We can use getInsertionIndexOfTab(currentGroupSelectedTab)
        // to determine the new position, instead of manual offset math and looking up
        // adjacent tabs.

        // Find the tab which was in the destination index before this move. Use
        // that tab to figure out the new position.
        int destinationTabIndex =
                tabModelNewIndex > tabModelOldIndex
                        ? tabModelNewIndex - relatedTabs.size()
                        : tabModelNewIndex + 1;
        Tab destinationTab = tabModel.getTabAt(destinationTabIndex);
        assumeNonNull(destinationTab);
        Tab destinationGroupSelectedTab =
                TabGroupUtils.getSelectedTabInGroupForTab(tabModel, destinationTab);
        int newPosition = mModelList.indexFromTabId(destinationGroupSelectedTab.getId());
        if (newPosition == TabModel.INVALID_TAB_INDEX) {
            int indexToUpdate =
                    mModelList.indexOfNthTabCard(
                            tabModel.representativeIndexOf(destinationTab)
                                    + (tabModelNewIndex > tabModelOldIndex ? 1 : -1));
            mModelList.updateTabListModelIdForGroup(destinationGroupSelectedTab, indexToUpdate);
            newPosition = mModelList.indexFromTabId(destinationGroupSelectedTab.getId());
        }
        mModelList.moveItem(curPosition, newPosition);
    }

    @Override
    public void didCreateNewGroup(Tab destinationTab, TabModel tabModel) {
        // On new group creation for the tab group representation in the GTS, update
        // the tab group color icon.
        int groupIndex = tabModel.representativeIndexOf(destinationTab);
        Tab groupTab = tabModel.getRepresentativeTabAt(groupIndex);
        assumeNonNull(groupTab);
        PropertyModel model = mModelList.getModelFromTabId(groupTab.getId());

        if (model != null) {
            Token tabGroupId = destinationTab.getTabGroupId();
            assumeNonNull(tabGroupId);
            @TabGroupColorId int colorId = tabModel.getTabGroupColorWithFallback(tabGroupId);
            mMediator.updateTabGroupProperties(destinationTab, model, colorId);
            mMediator.updateFaviconForTab(model, groupTab, null, null);
        }
    }
}
