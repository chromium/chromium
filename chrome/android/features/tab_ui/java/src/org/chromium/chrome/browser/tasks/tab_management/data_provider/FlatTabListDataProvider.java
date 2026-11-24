// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.base.Token;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.function.Predicate;

/**
 * {@link TabListDataProvider} implementation that displays tabs from one or more {@link TabModel}s
 * as a flat list of {@link TabItem} entries.
 *
 * <p>The {@link Predicate} filter decides which tabs are shown. Without a {@link Comparator}, the
 * provider keeps its model's tab order. With a {@link Comparator}, it sorts the tabs by it, and a
 * tab moving inside its model doesn't move its card. Observing several models also needs {@code
 * supportsMultipleModels} and a highlight supplier, and their tabs are merged into one sorted list.
 */
@NullMarked
public class FlatTabListDataProvider extends TabListDataProvider {
    private final @Nullable Predicate<Tab> mFilter;
    private final @Nullable Comparator<Tab> mComparator;
    private final boolean mSupportsMultipleModels;

    /**
     * Constructs a new {@link FlatTabListDataProvider}.
     *
     * @param tabModelListSupplier Supplier of the {@link TabModel}s to show tabs from.
     * @param highlightTabModelSupplier Supplier of the {@link TabModel} whose current tab is
     *     highlighted, holding null for no highlight. Required with {@code supportsMultipleModels}.
     *     When null, the only model's current tab is highlighted.
     * @param filter Predicate to filter tabs, or null for all tabs.
     * @param comparator Order to show tabs in, or null to use the model's order.
     * @param supportsMultipleModels Whether the supplier may hold more than one model. Requires a
     *     comparator, since one model's tab order can't place tabs from another, and a highlight
     *     supplier.
     */
    public FlatTabListDataProvider(
            NonNullObservableSupplier<List<TabModel>> tabModelListSupplier,
            @Nullable NullableObservableSupplier<TabModel> highlightTabModelSupplier,
            @Nullable Predicate<Tab> filter,
            @Nullable Comparator<Tab> comparator,
            boolean supportsMultipleModels) {
        super(tabModelListSupplier, highlightTabModelSupplier);
        assert !supportsMultipleModels || comparator != null
                : "Supporting several TabModels needs a comparator.";
        mFilter = filter;
        mComparator = comparator;
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
                        // active filter, and leaves them in place when there's a comparator.
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
                        onRestoreCompleted();
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

        List<Tab> tabs = new ArrayList<>();
        for (TabModel model : getAttachedTabModels()) {
            if (!model.isTabStateInitialized()) continue;
            for (Tab tab : model) {
                if (shouldShowTab(tab)) tabs.add(tab);
            }
        }
        if (mComparator != null) tabs.sort(mComparator);

        Tab selectedTab = getSelectedTab();
        for (Tab tab : tabs) {
            mItems.add(createTabItem(selectedTab, tab));
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

    @Override
    protected void moveTabItem(Tab tab) {
        // With a comparator, a tab's position in its model doesn't decide where its card goes, so a
        // model move leaves the card in place. Re-placing it could also swap tabs that compare
        // equal.
        if (mComparator != null) return;
        super.moveTabItem(tab);
    }

    @Override
    protected int getInsertionIndex(TabModel model, TabListItem targetItem) {
        Comparator<Tab> comparator = mComparator;
        if (comparator == null) return super.getInsertionIndex(model, targetItem);

        Tab targetTab = getTabForItem(targetItem);
        if (targetTab == null) return TabList.INVALID_TAB_INDEX;

        // Keep mItems sorted by the comparator: return the first index whose tab compares greater
        // than the target tab, or the end of the list.
        for (int i = 0; i < mItems.size(); i++) {
            Tab tab = getTabForItem(mItems.get(i));
            if (tab != null && comparator.compare(targetTab, tab) < 0) return i;
        }
        return mItems.size();
    }

    /** Returns the tab shown by {@code item}, or null if it isn't a tab card or isn't found. */
    private @Nullable Tab getTabForItem(TabListItem item) {
        return item instanceof TabItem tabItem ? getTabById(tabItem.getTabId()) : null;
    }
}
