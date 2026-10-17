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
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabListDataObserver.PayloadType;

/**
 * {@link TabListDataProvider} implementation that projects a {@link TabModel} into a sequence of
 * {@link TabGroupItem} and {@link TabItem} entries (collapsed-only for the Grid Tab Switcher, or
 * expandable with child {@link TabItem}s for Vertical Tabs).
 */
@NullMarked
public class GroupedTabListDataProvider extends TabListDataProvider {
    private final boolean mSupportsGroupExpansion;

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
                    public void restoreCompleted() {
                        requestDataReset();
                    }
                };

        // TODO(crbug.com/562590772): Wire incremental TabModelObserver and TabGroupObserver
        // callbacks for tab/group additions, removals, merges, moves, collapse, and selection.
        TabGroupObserver tabGroupObserver = new TabGroupObserver() {};

        initObservers(tabModelObserver, tabGroupObserver);
    }

    @Override
    protected void rebuildItems() {
        mItems.clear();
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

    /**
     * Syncs a newly added or restored {@code tab}. Inserting/updating the group header first lets
     * {@link #addTabItem} place the first child of an expanded group directly after its {@link
     * TabGroupItem} header.
     */
    private void syncTabOrGroup(Tab tab) {
        syncGroupItem(tab.getTabGroupId());
        addTabItem(tab);
    }

    /** Inserts the {@link TabGroupItem} if absent, or updates it if already present. */
    private void syncGroupItem(@Nullable Token tabGroupId) {
        if (tabGroupId == null) return;

        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return;

        int index = indexOfTabGroupId(tabGroupId);
        if (index != TabList.INVALID_TAB_INDEX) {
            updateGroupItemAt(model, index, tabGroupId, PayloadType.GROUP_MEMBERSHIP);
            return;
        }
        insertItem(model, createGroupItem(model, TabModelUtils.getCurrentTab(model), tabGroupId));
    }

    private void updateGroupItemAt(
            TabModel model, int index, Token tabGroupId, @PayloadType int payload) {
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
