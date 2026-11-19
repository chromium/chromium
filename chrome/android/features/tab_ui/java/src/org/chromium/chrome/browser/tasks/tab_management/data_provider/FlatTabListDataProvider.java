// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.base.Token;
import org.chromium.base.supplier.NonNullObservableSupplier;
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

import java.util.Comparator;
import java.util.List;
import java.util.function.Predicate;

/**
 * {@link TabListDataProvider} implementation that filters tabs from a {@link TabModel} according to
 * a {@link Predicate} into a flat list of {@link TabItem} entries.
 */
@NullMarked
public class FlatTabListDataProvider extends TabListDataProvider {
    private final @Nullable Predicate<Tab> mFilter;
    private final boolean mSupportsMultipleModels;

    /**
     * Constructs a new {@link FlatTabListDataProvider}.
     *
     * @param tabModelListSupplier Supplier of the {@link TabModel}s to show tabs from.
     * @param filter Predicate to filter tabs, or null for all tabs.
     * @param comparator Order to show tabs in, or null to use the model's order.
     * @param supportsMultipleModels Whether the supplier may hold more than one model. Requires a
     *     comparator, since one model's tab order can't place tabs from another.
     */
    public FlatTabListDataProvider(
            NonNullObservableSupplier<List<TabModel>> tabModelListSupplier,
            @Nullable Predicate<Tab> filter,
            @Nullable Comparator<Tab> comparator,
            boolean supportsMultipleModels) {
        super(tabModelListSupplier);
        assert !supportsMultipleModels || comparator != null
                : "Supporting several TabModels needs a comparator.";
        mFilter = filter;
        mSupportsMultipleModels = supportsMultipleModels;

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
                        // Intra-group reorders and group merges/ungroups are handled by
                        // TabGroupObserver; moveTabItem reorders standalone tabs when matching the
                        // active filter.
                        if (curIndex == newIndex || tab.getTabGroupId() != null) return;
                        moveTabItem(tab);
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
                        notifyTabClosureUndone(tab);
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        // For metrics.
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

        // Note: onTabGroupMoved is intentionally not overridden. It may be needed in the future if
        // a multi-group filter (e.g. TabListEditor)
        // requires reordering on whole-group moves.
        TabGroupObserver tabGroupObserver =
                new TabGroupObserver() {
                    @Override
                    public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
                        syncTabItem(movedTab);
                    }

                    @Override
                    public void didMoveWithinGroup(
                            Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                        if (tabModelNewIndex == tabModelOldIndex) return;
                        moveTabItem(movedTab);
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
        // TODO(crbug.com/587126481): Build items from every attached model and sort them by
        // mComparator.
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
    protected boolean isTabInScope(Tab tab) {
        return mFilter == null || mFilter.test(tab);
    }

    @Override
    protected boolean shouldShowTab(Tab tab) {
        return isTabInScope(tab);
    }

    @Override
    protected boolean supportsMultipleModels() {
        return mSupportsMultipleModels;
    }
}
