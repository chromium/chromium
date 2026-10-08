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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.function.Predicate;

/**
 * {@link TabListDataProvider} implementation that filters tabs from a {@link TabModel} according to
 * a {@link Predicate} into a flat list of {@link TabItem} entries.
 */
@NullMarked
public class FlatTabListDataProvider extends TabListDataProvider {
    private final @Nullable Predicate<Tab> mFilter;

    /**
     * Constructs a new {@link FlatTabListDataProvider}.
     *
     * @param tabModelSupplier Supplier of the current {@link TabModel}.
     * @param filter Predicate to filter tabs, or null for all tabs.
     */
    public FlatTabListDataProvider(
            NullableObservableSupplier<TabModel> tabModelSupplier,
            @Nullable Predicate<Tab> filter) {
        super(tabModelSupplier);
        // TODO(crbug.com/562590772): Wire flat surface coordinators to pass this provider to
        // TabListCoordinator with their surface filter (e.g.
        // `tab -> groupId.equals(tab.getTabGroupId())` for TabGridDialog/TabGroupUi,
        // `Tab::getIsPinned` for pinned tabs, or `tab -> tabIds.contains(tab.getId())` for
        // TabListEditor).
        mFilter = filter;

        TabModelObserver tabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        addTabItem(tab);
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        // Grouped tab moves and intra-group reorders are handled by
                        // mTabGroupObserver; moveTabItem reorders standalone tabs when matching the
                        // active filter.
                        if (tab.getTabGroupId() == null) {
                            moveTabItem(tab.getId());
                        }
                    }

                    @Override
                    public void didRemoveTabForClosure(Tab tab) {
                        removeTabItem(tab.getId());
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        removeTabItem(tab.getId());
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        // Restores the undone tab if it matches the filter, or no-ops otherwise.
                        syncTabItem(tab);
                        // For metrics.
                        notifyObservers(obs -> obs.onTabClosureUndone(tab.getId()));
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        // For metrics.
                        notifyObservers(obs -> obs.onTabClosureCommitted(tab.getId()));
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
                };

        TabGroupObserver tabGroupObserver =
                new TabGroupObserver() {
                    @Override
                    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
                        syncTabItem(movedTab);
                    }

                    @Override
                    public void didMoveWithinGroup(
                            Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                        moveTabItem(movedTab.getId());
                    }

                    @Override
                    public void didMoveTabOutOfGroup(Tab movedTab, Token oldTabGroupId) {
                        syncTabItem(movedTab);
                    }
                };

        initObservers(tabModelObserver, tabGroupObserver);
    }

    @Override
    protected void rebuildItems() {
        mItems.clear();
        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return;

        Tab selectedTab = TabModelUtils.getCurrentTab(model);
        for (Tab tab : model) {
            if (shouldShowTab(tab)) {
                mItems.add(createTabItem(selectedTab, tab));
            }
        }
    }

    @Override
    protected boolean shouldShowTab(Tab tab) {
        return mFilter == null || mFilter.test(tab);
    }
}
