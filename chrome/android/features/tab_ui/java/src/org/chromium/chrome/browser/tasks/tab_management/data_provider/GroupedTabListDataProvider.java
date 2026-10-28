// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.base.Token;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabListDataObserver.PayloadType;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * {@link TabListDataProvider} implementation that projects a {@link TabModel} into a sequence of
 * {@link TabGroupItem} and {@link TabItem} entries (collapsed-only for the Grid Tab Switcher, or
 * expandable with child {@link TabItem}s for Vertical Tabs).
 */
@NullMarked
public class GroupedTabListDataProvider extends TabListDataProvider {
    private final boolean mSupportsGroupExpansion;
    private final Set<Token> mRemovingTabGroupIds = new HashSet<>();

    /**
     * Constructs a new {@link GroupedTabListDataProvider}.
     *
     * @param tabModelSupplier Supplier of the active {@link TabModel}.
     * @param supportsGroupExpansion Whether tab groups can expand to show child {@link TabItem}s
     *     (true for Vertical Tabs, false for Grid Tab Switcher).
     */
    public GroupedTabListDataProvider(
            NullableObservableSupplier<TabModel> tabModelSupplier, boolean supportsGroupExpansion) {
        // TODO(crbug.com/562590772): Wire TabSwitcherPaneCoordinator (supportsGroupExpansion=false)
        // and VerticalTabListCoordinator (supportsGroupExpansion=true) to pass this provider to
        // TabListCoordinator.
        super(tabModelSupplier);
        mSupportsGroupExpansion = supportsGroupExpansion;

        TabModelObserver tabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        // Grouped tabs added via groupWithParent fire didMergeTabToGroup
                        // immediately after didAddTab; only restore and reparenting add
                        // pre-grouped tabs without it.
                        if (tab.getTabGroupId() != null
                                && type != TabLaunchType.FROM_RESTORE
                                && type != TabLaunchType.FROM_REPARENTING
                                && type != TabLaunchType.FROM_REPARENTING_BACKGROUND) {
                            return;
                        }
                        syncTabOrGroup(tab);
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        // Grouped tab moves and intra-group reorders are handled by
                        // TabGroupObserver; moveTabItem reorders standalone tabs to their updated
                        // TabModel position.
                        if (curIndex == newIndex || tab.getTabGroupId() != null) return;
                        moveTabItem(tab);
                    }

                    @Override
                    public void didRemoveTabForClosure(Tab tab) {
                        removeTabAndUpdateGroup(tab);
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        removeTabAndUpdateGroup(tab);
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        syncTabOrGroup(tab);
                        notifyTabClosureUndone(tab);
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        notifyTabClosureCommitted(tab);
                    }

                    @Override
                    public void restoreCompleted() {
                        requestDataReset();
                    }

                    @Override
                    public void didSelectTab(
                            Tab tab, @TabSelectionType int type, @TabId int lastId) {
                        selectTab(tab, /* prevSelectedTabId= */ lastId);
                    }

                    @Override
                    public void didChangePinState(Tab tab) {
                        updatePinState(tab);
                    }

                    @Override
                    public void onTabsSelectionChanged() {
                        updateMultiSelection();
                    }
                };

        // TODO(crbug.com/562590772): Wire incremental TabModelObserver and TabGroupObserver
        // callbacks for tab/group additions, removals, merges, moves, collapse, and selection.
        TabGroupObserver tabGroupObserver =
                new TabGroupObserver() {
                    @Override
                    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
                        TabItem existingItem = syncTabItem(movedTab);
                        updateTabItem(existingItem, PayloadType.GROUP_MEMBERSHIP);
                        syncGroupItem(movedTab.getTabGroupId());
                    }

                    @Override
                    public void didMoveTabOutOfGroup(Tab movedTab, Token oldTabGroupId) {
                        // Sync movedTab only when ungrouped to a standalone tab; group-to-group
                        // merges follow with didMergeTabToGroup.
                        if (movedTab.getTabGroupId() == null) {
                            TabItem existingItem = syncTabItem(movedTab);
                            updateTabItem(existingItem, PayloadType.GROUP_MEMBERSHIP);
                        }
                        updateGroupItem(oldTabGroupId, PayloadType.GROUP_MEMBERSHIP);
                    }

                    @Override
                    public void onTabGroupMoved(
                            Token tabGroupId, int tabModelOldIndex, int tabModelNewIndex) {
                        if (tabModelNewIndex == tabModelOldIndex) return;
                        moveGroupItem(tabGroupId);
                    }

                    @Override
                    public void didMoveWithinGroup(
                            Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                        if (tabModelNewIndex == tabModelOldIndex) return;
                        moveTabItem(movedTab);
                        updateGroupItem(movedTab.getTabGroupId(), PayloadType.TAB_GROUP_ORDER);
                    }

                    @Override
                    public void onTabGroupRemoving(Token tabGroupId) {
                        mRemovingTabGroupIds.add(tabGroupId);
                    }

                    @Override
                    public void didRemoveTabGroup(
                            Token oldTabGroupId, @DidRemoveTabGroupReason int removalReason) {
                        mRemovingTabGroupIds.remove(oldTabGroupId);
                        removeGroupItem(oldTabGroupId);
                    }

                    @Override
                    public void didChangeTabGroupTitle(Token tabGroupId, String newTitle) {
                        updateGroupItem(tabGroupId, PayloadType.TITLE);
                    }

                    @Override
                    public void didChangeTabGroupColor(
                            Token tabGroupId, @TabGroupColorId int newColor) {
                        updateGroupItem(tabGroupId, PayloadType.COLOR);
                    }

                    @Override
                    public void didChangeTabGroupCollapsed(
                            Token tabGroupId, boolean isCollapsed, boolean animate) {
                        if (!mSupportsGroupExpansion) return;
                        updateGroupCollapsed(tabGroupId, isCollapsed);
                    }
                };

        initObservers(tabModelObserver, tabGroupObserver);
    }

    @Override
    protected void rebuildItems() {
        mItems.clear();
        mRemovingTabGroupIds.clear();
        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return;

        Tab selectedTab = TabModelUtils.getCurrentTab(model);
        Token lastGroupId = null;
        boolean isCurrentGroupCollapsed = true;
        for (Tab tab : model) {
            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId != null) {
                // Add a TabGroupItem for the first tab of each contiguous group and cache its
                // collapse state for the remaining tabs in the group.
                if (!tabGroupId.equals(lastGroupId)) {
                    TabGroupItem groupItem = createGroupItem(model, selectedTab, tabGroupId);
                    mItems.add(groupItem);
                    lastGroupId = tabGroupId;
                    isCurrentGroupCollapsed = groupItem.isCollapsed();
                }
                // Expanded groups also include a child TabItem for each member tab.
                if (!isCurrentGroupCollapsed) {
                    mItems.add(createTabItem(selectedTab, tab));
                }
            } else {
                mItems.add(createTabItem(selectedTab, tab));
                lastGroupId = null;
            }
        }
    }

    @Override
    protected boolean shouldShowTab(Tab tab) {
        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return false;
        Token tabGroupId = tab.getTabGroupId();
        return tabGroupId == null || !isGroupCollapsed(model, tabGroupId);
    }

    @Override
    protected int indexOfItemForTab(TabModel model, @TabId int tabId) {
        int index = super.indexOfItemForTab(model, tabId);
        if (index != TabList.INVALID_TAB_INDEX) return index;

        // In collapsed-only surfaces (GTS), group tabs map to their parent TabGroupItem.
        if (!mSupportsGroupExpansion) {
            Tab tab = model.getTabById(tabId);
            Token groupId = tab != null ? tab.getTabGroupId() : null;
            if (groupId != null) {
                return indexOfTabGroupId(groupId);
            }
        }
        return TabList.INVALID_TAB_INDEX;
    }

    /**
     * Syncs a newly added or restored {@code tab}. Inserting/updating the group header first lets
     * {@link #addTabItem} place the first child of an expanded group directly after its {@link
     * TabGroupItem} header.
     */
    private void syncTabOrGroup(Tab tab) {
        syncGroupItem(tab.getTabGroupId());
        addTabItem(tab);
    }

    private void removeTabAndUpdateGroup(Tab tab) {
        // Unconditionally remove the tab item (no-op when collapsed, removes the child row when
        // expanded) and refresh the parent group header if grouped.
        removeTabItem(tab.getId());
        updateGroupItem(tab.getTabGroupId(), PayloadType.GROUP_MEMBERSHIP);
    }

    /** Inserts the {@link TabGroupItem} if absent, or updates it if already present. */
    private void syncGroupItem(@Nullable Token tabGroupId) {
        if (tabGroupId == null) return;

        int index = indexOfTabGroupId(tabGroupId);
        if (index != TabList.INVALID_TAB_INDEX) {
            updateGroupItemAt(index, tabGroupId, PayloadType.GROUP_MEMBERSHIP);
            return;
        }

        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return;
        insertItem(model, createGroupItem(model, TabModelUtils.getCurrentTab(model), tabGroupId));
    }

    /** Updates the existing {@link TabGroupItem} in {@link #mItems} and notifies observers. */
    private void updateGroupItem(@Nullable Token tabGroupId, @PayloadType int payload) {
        if (tabGroupId == null) return;
        int index = indexOfTabGroupId(tabGroupId);
        if (index == TabList.INVALID_TAB_INDEX) return;
        updateGroupItemAt(index, tabGroupId, payload);
    }

    private void updateGroupItemAt(int index, Token tabGroupId, @PayloadType int payload) {
        TabModel model = getTabModelIfTabStateInitialized();
        // Skip updating if the group is being removed or no longer exists in TabModel
        // (didRemoveTabGroup will remove the group card next).
        if (model == null
                || !model.containsTabGroup(tabGroupId)
                || mRemovingTabGroupIds.contains(tabGroupId)) {
            return;
        }

        TabGroupItem oldItem = (TabGroupItem) mItems.get(index);
        TabGroupItem updatedItem =
                oldItem.withSelected(
                        isGroupSelected(TabModelUtils.getCurrentTab(model), tabGroupId));
        updateItemAt(index, updatedItem, payload);
    }

    private TabGroupItem createGroupItem(
            TabModel model, @Nullable Tab selectedTab, Token tabGroupId) {
        return new TabGroupItem(
                tabGroupId,
                isGroupSelected(selectedTab, tabGroupId),
                isGroupCollapsed(model, tabGroupId));
    }

    /**
     * Removes the {@link TabGroupItem} for {@code tabGroupId} from {@link #mItems} and notifies
     * observers via {@link TabListDataObserver#onItemsRemoved}.
     *
     * @param tabGroupId The ID of the tab group to remove.
     */
    private void removeGroupItem(Token tabGroupId) {
        int index = indexOfTabGroupId(tabGroupId);
        if (index == TabList.INVALID_TAB_INDEX) return;

        removeItemsAt(index, /* count= */ 1);
    }

    /**
     * Moves the {@link TabGroupItem} (and all child {@link TabItem}s if expanded) for {@code
     * tabGroupId} to its updated {@link TabModel} position and notifies observers via {@link
     * TabListDataObserver#onItemsMoved}.
     *
     * @param tabGroupId The ID of the tab group to move.
     */
    private void moveGroupItem(Token tabGroupId) {
        int index = indexOfTabGroupId(tabGroupId);
        if (index == TabList.INVALID_TAB_INDEX) return;

        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null
                || !model.containsTabGroup(tabGroupId)
                || mRemovingTabGroupIds.contains(tabGroupId)) {
            return;
        }

        int count =
                isGroupCollapsed(model, tabGroupId) ? 1 : 1 + model.getTabCountForGroup(tabGroupId);
        moveItemsAt(index, count);
    }

    private void updateGroupCollapsed(Token tabGroupId, boolean isCollapsed) {
        int headerIndex = indexOfTabGroupId(tabGroupId);
        if (headerIndex == TabList.INVALID_TAB_INDEX) return;

        TabGroupItem groupItem = (TabGroupItem) mItems.get(headerIndex);
        if (groupItem.isCollapsed() == isCollapsed) return;

        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null
                || !model.containsTabGroup(tabGroupId)
                || mRemovingTabGroupIds.contains(tabGroupId)) {
            return;
        }

        TabGroupItem updatedGroupItem = groupItem.withCollapsed(isCollapsed);
        updateItemAt(headerIndex, updatedGroupItem, PayloadType.COLLAPSED);

        if (isCollapsed) {
            removeItemsAt(headerIndex + 1, model.getTabCountForGroup(tabGroupId));
        } else {
            List<Tab> children = model.getTabsInGroup(tabGroupId);
            Tab selectedTab = TabModelUtils.getCurrentTab(model);
            List<TabItem> childItems = new ArrayList<>(children.size());
            for (Tab childTab : children) {
                childItems.add(createTabItem(selectedTab, childTab));
            }
            insertItemsAt(headerIndex + 1, childItems);
        }
    }

    private boolean isGroupCollapsed(TabModel model, Token tabGroupId) {
        return !mSupportsGroupExpansion || model.getTabGroupCollapsed(tabGroupId);
    }

    private boolean isGroupSelected(@Nullable Tab selectedTab, Token tabGroupId) {
        // Group headers are only selectable in collapsed-only surfaces; in expandable
        // surfaces, selection is shown on the child TabItem.
        return !mSupportsGroupExpansion
                && selectedTab != null
                && tabGroupId.equals(selectedTab.getTabGroupId());
    }

    private int indexOfTabGroupId(Token tabGroupId) {
        for (int i = 0; i < mItems.size(); i++) {
            if (matchesTabGroupId(mItems.get(i), tabGroupId)) return i;
        }
        return TabList.INVALID_TAB_INDEX;
    }
}
