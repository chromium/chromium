// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.data_provider.TabListDataObserver.PayloadType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Base class providing {@link TabListItem} entries for tab list surfaces. */
@NullMarked
public abstract class TabListDataProvider {
    /** The ordered list of {@link TabListItem} entries populated by {@link #rebuildItems()}. */
    protected final List<TabListItem> mItems = new ArrayList<>();

    private final ObserverList<TabListDataObserver> mObservers = new ObserverList<>();
    private final NullableObservableSupplier<TabModel> mTabModelSupplier;
    private final Callback<@Nullable TabModel> mOnTabModelChanged = this::onTabModelChanged;

    private @Nullable TabModel mAttachedTabModel;
    private @Nullable TabModelObserver mTabModelObserver;
    private @Nullable TabGroupObserver mTabGroupObserver;

    /**
     * Constructs a {@link TabListDataProvider}.
     *
     * @param tabModelSupplier Supplier of the current {@link TabModel}.
     */
    protected TabListDataProvider(NullableObservableSupplier<TabModel> tabModelSupplier) {
        // TODO(crbug.com/562590772): Surface coordinators (e.g. TabGridDialogCoordinator,
        // TabSwitcherPaneCoordinator) will construct a provider and pass it to TabListCoordinator,
        // which forwards it via TabListConfig to TabListMediator.
        mTabModelSupplier = tabModelSupplier;
    }

    /**
     * Adds an observer for data change events.
     *
     * @param observer The {@link TabListDataObserver} to add.
     */
    public void addObserver(TabListDataObserver observer) {
        // TODO(crbug.com/562590772): TabListMediator will call addObserver() in initWithNative()
        // and removeObserver() in destroy().
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer for data change events.
     *
     * @param observer The {@link TabListDataObserver} to remove.
     */
    public void removeObserver(TabListDataObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Attaches to the current {@link TabModel}, rebuilds the item list, and notifies observers via
     * {@link TabListDataObserver#onDataReset}.
     */
    public void requestDataReset() {
        // TODO(crbug.com/562590772): Sole external reset entry point.
        // TabListMediator#resetWithListOfTabs invokes this when a tab list surface opens or resets.
        attachToModel(mTabModelSupplier.get());
        rebuildItems();
        List<TabListItem> unmodifiableItems = Collections.unmodifiableList(mItems);
        notifyObservers(obs -> obs.onDataReset(unmodifiableItems));
    }

    /**
     * Detaches from the current {@link TabModel} and clears cached items when the surface hides.
     */
    public void stop() {
        // TODO(crbug.com/562590772): TabListMediator#removeObservers invokes this on
        // resetWithListOfTabs(null) and prepareHiding().
        detachFromCurrentModel();
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mObservers.clear();
        mTabModelSupplier.removeObserver(mOnTabModelChanged);
        detachFromCurrentModel();
        mTabModelObserver = null;
        mTabGroupObserver = null;
    }

    /**
     * Registers observers and starts observing {@link TabModel} switches. Must be called at the end
     * of the subclass constructor after subclass fields are initialized.
     *
     * @param tabModelObserver The {@link TabModelObserver} to register on the active {@link
     *     TabModel}.
     * @param tabGroupObserver The optional {@link TabGroupObserver} to register on the active
     *     {@link TabModel}.
     */
    protected void initObservers(
            TabModelObserver tabModelObserver, @Nullable TabGroupObserver tabGroupObserver) {
        mTabModelObserver = tabModelObserver;
        mTabGroupObserver = tabGroupObserver;
        mTabModelSupplier.addSyncObserver(mOnTabModelChanged);
    }

    /** Returns the current {@link TabModel} if tab state is initialized, or null otherwise. */
    protected @Nullable TabModel getTabModelIfTabStateInitialized() {
        return mAttachedTabModel != null && mAttachedTabModel.isTabStateInitialized()
                ? mAttachedTabModel
                : null;
    }

    /** Rebuilds {@link #mItems} from the current {@link TabModel}. */
    protected abstract void rebuildItems();

    /**
     * Returns whether {@code tab} is in scope for this data provider surface (either directly as a
     * {@link TabItem} or inside a composite group item). Subclasses with a filter predicate
     * override this to restrict surface scope.
     *
     * @param tab The {@link Tab} to evaluate.
     */
    protected boolean isTabInScope(Tab tab) {
        return true;
    }

    /**
     * Returns whether {@code tab} should have a visible {@link TabItem} in {@link #mItems}.
     *
     * @param tab The {@link Tab} to evaluate.
     */
    protected abstract boolean shouldShowTab(Tab tab);

    /**
     * Creates a {@link TabItem} from the given {@link Tab}.
     *
     * @param selectedTab The currently selected {@link Tab} in the model, or null if none.
     * @param tab The {@link Tab} to convert.
     * @return A new {@link TabItem} representing {@code tab}.
     */
    protected TabItem createTabItem(@Nullable Tab selectedTab, Tab tab) {
        TabModel model = getTabModelIfTabStateInitialized();
        return new TabItem(
                tab.getId(),
                tab.equals(selectedTab),
                tab.getIsPinned(),
                model != null && model.isTabMultiSelected(tab.getId()));
    }

    /**
     * Inserts a {@link TabItem} for {@code tab} at its {@link TabModel} position and notifies
     * observers via {@link TabListDataObserver#onItemsInserted}.
     *
     * @param tab The {@link Tab} to insert.
     */
    protected void addTabItem(Tab tab) {
        // Ignore tabs that are filtered out or already present in mItems.
        if (!shouldShowTab(tab)) return;

        @TabId int tabId = tab.getId();
        if (indexOfTabId(tabId) != TabList.INVALID_TAB_INDEX) return;

        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return;

        insertItem(model, createTabItem(TabModelUtils.getCurrentTab(model), tab));
    }

    /**
     * Removes the {@link TabItem} for {@code tabId} from {@link #mItems} and notifies observers via
     * {@link TabListDataObserver#onItemsRemoved}.
     *
     * @param tabId The ID of the tab to remove.
     */
    protected void removeTabItem(@TabId int tabId) {
        int index = indexOfTabId(tabId);
        if (index == TabList.INVALID_TAB_INDEX) return;

        removeItemsAt(index, /* count= */ 1);
    }

    /**
     * Moves the {@link TabItem} for {@code tab} to its updated {@link TabModel} position and
     * notifies observers via {@link TabListDataObserver#onItemsMoved}. No-ops if the tab should not
     * have a visible {@link TabItem} or is not present in {@link #mItems}.
     *
     * @param tab The {@link Tab} to move.
     */
    protected void moveTabItem(Tab tab) {
        // Ignore tabs moving out of scope; dedicated lifecycle events handle removal.
        if (!shouldShowTab(tab)) return;

        @TabId int tabId = tab.getId();
        int fromIndex = indexOfTabId(tabId);
        if (fromIndex == TabList.INVALID_TAB_INDEX) return;

        moveItemsAt(fromIndex, /* count= */ 1);
    }

    /**
     * Synchronizes {@code tab}'s {@link TabItem} in {@link #mItems} against {@link
     * #shouldShowTab(Tab)}. Removes the item if hidden, inserts it if newly visible, or repositions
     * and returns the existing {@link TabItem} if already present.
     *
     * @param tab The {@link Tab} to synchronize.
     * @return The existing {@link TabItem} if it was already in {@link #mItems} and remains
     *     visible, or null if the item was inserted, removed, or remained hidden.
     */
    protected @Nullable TabItem syncTabItem(Tab tab) {
        if (!shouldShowTab(tab)) {
            removeTabItem(tab.getId());
            return null;
        }

        int fromIndex = indexOfTabId(tab.getId());
        if (fromIndex == TabList.INVALID_TAB_INDEX) {
            addTabItem(tab);
            return null;
        }

        TabItem existingItem = (TabItem) mItems.get(fromIndex);
        moveTabItem(tab);
        return existingItem;
    }

    /**
     * Notifies observers via {@link TabListDataObserver#onItemUpdated} that {@code item} changed if
     * non-null.
     *
     * @param item The {@link TabItem} to update, or null to no-op.
     * @param payload The {@link PayloadType} indicating which property changed.
     */
    protected void updateTabItem(@Nullable TabItem item, @PayloadType int payload) {
        if (item == null) return;
        notifyObservers(obs -> obs.onItemUpdated(item, payload));
    }

    /**
     * Resolves the index in {@link #mItems} of the item representing {@code tabId}, or {@link
     * TabList#INVALID_TAB_INDEX} if absent. Subclasses can override to map grouped tabs to their
     * containing {@link TabGroupItem}.
     *
     * @param model The active {@link TabModel}.
     * @param tabId The ID of the tab to resolve.
     */
    protected int indexOfItemForTab(TabModel model, @TabId int tabId) {
        return indexOfTabId(tabId);
    }

    /**
     * Updates the selected item in {@link #mItems} when {@code tab} is selected in {@link
     * TabModel}.
     *
     * @param tab The newly selected {@link Tab}.
     * @param prevSelectedTabId The ID of the previously selected tab, or {@link
     *     Tab#INVALID_TAB_ID}.
     */
    protected void selectTab(Tab tab, @TabId int prevSelectedTabId) {
        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return;

        @TabId int newSelectedTabId = tab.getId();
        if (newSelectedTabId == prevSelectedTabId) return;

        int prevSelectedIndex = indexOfItemForTab(model, prevSelectedTabId);
        int newSelectedIndex = indexOfItemForTab(model, newSelectedTabId);
        if (newSelectedIndex != TabList.INVALID_TAB_INDEX
                && newSelectedIndex == prevSelectedIndex) {
            return;
        }

        // Deselect all other tabs first so only one tab remains selected.
        for (int i = 0; i < mItems.size(); i++) {
            if (i != newSelectedIndex && mItems.get(i).isSelected()) {
                updateSelection(i, /* isSelected= */ false);
            }
        }

        if (newSelectedIndex != TabList.INVALID_TAB_INDEX) {
            updateSelection(newSelectedIndex, /* isSelected= */ true);
        }
    }

    /**
     * Updates the pinned state of {@code tab} in {@link #mItems} when its pin state changes in
     * {@link TabModel}.
     *
     * @param tab The {@link Tab} whose pin state changed.
     */
    protected void updatePinState(Tab tab) {
        if (getTabModelIfTabStateInitialized() == null) return;

        // Remove if the pin change filtered the tab out.
        if (!shouldShowTab(tab)) {
            removeTabItem(tab.getId());
            return;
        }

        @TabId int tabId = tab.getId();
        int index = indexOfTabId(tabId);
        // Insert if the pin change made the tab visible under the filter.
        if (index == TabList.INVALID_TAB_INDEX) {
            addTabItem(tab);
            return;
        }

        if (mItems.get(index) instanceof TabItem tabItem) {
            boolean isPinned = tab.getIsPinned();
            if (tabItem.isPinned() != isPinned) {
                updateItemAt(index, tabItem.withPinned(isPinned), PayloadType.PIN_STATE);
            }
        }
    }

    /**
     * Updates the multi-selected state of all {@link TabItem}s in {@link #mItems} when the set of
     * multi-selected tabs changes in {@link TabModel}.
     */
    protected void updateMultiSelection() {
        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return;

        for (int i = 0; i < mItems.size(); i++) {
            if (mItems.get(i) instanceof TabItem tabItem) {
                boolean isMultiSelected = model.isTabMultiSelected(tabItem.getTabId());
                if (tabItem.isMultiSelected() != isMultiSelected) {
                    updateItemAt(
                            i,
                            tabItem.withMultiSelected(isMultiSelected),
                            PayloadType.MULTI_SELECTION);
                }
            }
        }
    }

    /**
     * Notifies observers that a tab closure was undone if the tab is in scope.
     *
     * @param tab The {@link Tab} whose closure was undone.
     */
    protected void notifyTabClosureUndone(Tab tab) {
        if (!isTabInScope(tab)) return;
        notifyObservers(obs -> obs.onTabClosureUndone(tab.getId()));
    }

    /**
     * Notifies observers that a tab closure was committed if the tab is in scope.
     *
     * @param tab The {@link Tab} whose closure was committed.
     */
    protected void notifyTabClosureCommitted(Tab tab) {
        if (!isTabInScope(tab)) return;
        notifyObservers(obs -> obs.onTabClosureCommitted(tab.getId()));
    }

    /**
     * Inserts a {@link TabListItem} into {@link #mItems} at its {@link TabModel} position and
     * notifies observers via {@link TabListDataObserver#onItemsInserted}.
     *
     * @param model The active {@link TabModel}.
     * @param item The {@link TabListItem} to insert.
     */
    protected void insertItem(TabModel model, TabListItem item) {
        insertItemsAt(getInsertionIndex(model, item), List.of(item));
    }

    /**
     * Inserts {@code items} starting at {@code index} in {@link #mItems} and notifies observers via
     * {@link TabListDataObserver#onItemsInserted}.
     *
     * @param index The insertion index in {@link #mItems}.
     * @param items The contiguous {@link TabListItem} entries to insert.
     */
    protected void insertItemsAt(int index, List<? extends TabListItem> items) {
        if (!isValidInsertionIndex(index) || items.isEmpty()) {
            return;
        }

        // Pass the preceding item as an anchor (or null for the start of the tab region) so UI
        // consumers can insert relative to it without index offset math around non-tab items.
        TabListItem previousItem = getPreviousItem(index);
        List<TabListItem> insertedItems = List.copyOf(items);
        mItems.addAll(index, insertedItems);
        notifyObservers(obs -> obs.onItemsInserted(insertedItems, previousItem));
    }

    /**
     * Replaces the {@link TabListItem} at {@code index} in {@link #mItems} with {@code updatedItem}
     * and notifies observers via {@link TabListDataObserver#onItemUpdated}.
     *
     * @param index The index in {@link #mItems} to update.
     * @param updatedItem The updated {@link TabListItem} to store at {@code index}.
     * @param payload The {@link PayloadType} indicating which property changed.
     */
    protected void updateItemAt(int index, TabListItem updatedItem, @PayloadType int payload) {
        if (!isValidIndex(index)) return;

        mItems.set(index, updatedItem);
        notifyObservers(obs -> obs.onItemUpdated(updatedItem, payload));
    }

    /**
     * Removes {@code count} {@link TabListItem} entries starting at {@code startIndex} and notifies
     * observers via {@link TabListDataObserver#onItemsRemoved}.
     *
     * @param startIndex The starting index in {@link #mItems}.
     * @param count The number of contiguous items to remove.
     */
    protected void removeItemsAt(int startIndex, int count) {
        if (count <= 0 || !isValidIndex(startIndex) || startIndex + count > mItems.size()) {
            return;
        }

        List<TabListItem> subList = mItems.subList(startIndex, startIndex + count);
        List<TabListItem> removedItems = List.copyOf(subList);
        subList.clear();
        notifyObservers(obs -> obs.onItemsRemoved(removedItems));
    }

    /**
     * Moves {@code count} contiguous {@link TabListItem} entries starting at {@code startIndex} to
     * their updated {@link TabModel} position and notifies observers via {@link
     * TabListDataObserver#onItemsMoved}.
     *
     * @param startIndex The starting index in {@link #mItems}.
     * @param count The number of contiguous items to move.
     */
    protected void moveItemsAt(int startIndex, int count) {
        if (count <= 0 || !isValidIndex(startIndex) || startIndex + count > mItems.size()) {
            return;
        }

        TabModel model = getTabModelIfTabStateInitialized();
        if (model == null) return;

        TabListItem targetItem = mItems.get(startIndex);
        List<TabListItem> subList = mItems.subList(startIndex, startIndex + count);
        List<TabListItem> movedItems = List.copyOf(subList);
        subList.clear();

        int toIndex = getInsertionIndex(model, targetItem);
        int insertIndex = toIndex != TabList.INVALID_TAB_INDEX ? toIndex : startIndex;
        mItems.addAll(insertIndex, movedItems);

        // Skip observer notification if the items were restored at startIndex or did not move.
        if (toIndex == TabList.INVALID_TAB_INDEX || startIndex == toIndex) return;

        TabListItem previousItem = getPreviousItem(insertIndex);
        notifyObservers(obs -> obs.onItemsMoved(movedItems, previousItem));
    }

    /** Returns whether {@code index} is a valid index in {@link #mItems}. */
    private boolean isValidIndex(int index) {
        return index >= 0 && index < mItems.size();
    }

    /** Returns whether {@code index} is a valid insertion position in {@link #mItems}. */
    private boolean isValidInsertionIndex(int index) {
        return isValidIndex(index) || index == mItems.size();
    }

    /**
     * Returns whether {@code item} is a {@link TabGroupItem} matching {@code tabGroupId}.
     *
     * @param item The {@link TabListItem} to check.
     * @param tabGroupId The tab group {@link Token} to match against, or null.
     */
    protected static boolean matchesTabGroupId(TabListItem item, @Nullable Token tabGroupId) {
        return tabGroupId != null
                && item instanceof TabGroupItem groupItem
                && groupItem.getTabGroupId().equals(tabGroupId);
    }

    private void updateSelection(int index, boolean isSelected) {
        TabListItem currentItem = mItems.get(index);
        if (currentItem.isSelected() == isSelected) return;

        updateItemAt(index, currentItem.withSelected(isSelected), PayloadType.SELECTION);
    }

    private int indexOfTabId(@TabId int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return TabList.INVALID_TAB_INDEX;
        for (int i = 0; i < mItems.size(); i++) {
            if (matchesTabId(mItems.get(i), tabId)) return i;
        }
        return TabList.INVALID_TAB_INDEX;
    }

    private int getInsertionIndex(TabModel model, TabListItem targetItem) {
        int itemIndex = 0;
        for (Tab tab : model) {
            int tabId = tab.getId();
            Token tabGroupId = tab.getTabGroupId();
            // A TabGroupItem is placed at the position of its first member tab in TabModel.
            if (matchesTabGroupId(targetItem, tabGroupId)) return itemIndex;
            // Advance past an existing group header so its child tabs land after the header.
            if (itemIndex < mItems.size() && matchesTabGroupId(mItems.get(itemIndex), tabGroupId)) {
                itemIndex++;
            }
            if (matchesTabId(targetItem, tabId)) return itemIndex;
            // Advance past tabs that are already projected in mItems; filtered-out tabs are
            // skipped without incrementing itemIndex.
            if (itemIndex < mItems.size() && matchesTabId(mItems.get(itemIndex), tabId)) {
                itemIndex++;
            }
        }
        return TabList.INVALID_TAB_INDEX;
    }

    /**
     * Returns the {@link TabListItem} immediately preceding {@code index}, or null if at the start.
     */
    private @Nullable TabListItem getPreviousItem(int index) {
        return index > 0 && index <= mItems.size() ? mItems.get(index - 1) : null;
    }

    private static boolean matchesTabId(TabListItem item, @TabId int tabId) {
        return item instanceof TabItem tabItem && tabItem.getTabId() == tabId;
    }

    /**
     * Dispatches {@code callback} to all registered {@link TabListDataObserver}s.
     *
     * @param callback The callback to invoke on each observer.
     */
    private void notifyObservers(Callback<TabListDataObserver> callback) {
        for (TabListDataObserver obs : mObservers) {
            callback.onResult(obs);
        }
    }

    private void onTabModelChanged(@Nullable TabModel newModel) {
        if (mAttachedTabModel == newModel) return;
        // Detach from the previous model on switch; the surface re-attaches on its next
        // requestDataReset(), matching TabListMediator#onTabModelChanged.
        detachFromCurrentModel();
    }

    private void attachToModel(@Nullable TabModel model) {
        if (mAttachedTabModel == model || mTabModelObserver == null) return;
        detachFromCurrentModel();
        mAttachedTabModel = model;
        if (mAttachedTabModel != null) {
            mAttachedTabModel.addObserver(mTabModelObserver);
            if (mTabGroupObserver != null) {
                mAttachedTabModel.addTabGroupObserver(mTabGroupObserver);
            }
        }
    }

    private void detachFromCurrentModel() {
        if (mAttachedTabModel != null) {
            if (mTabModelObserver != null) {
                mAttachedTabModel.removeObserver(mTabModelObserver);
            }
            if (mTabGroupObserver != null) {
                mAttachedTabModel.removeTabGroupObserver(mTabGroupObserver);
            }
        }
        mAttachedTabModel = null;
        mItems.clear();
    }

    List<TabListItem> getItemsForTesting() {
        return Collections.unmodifiableList(mItems);
    }
}
