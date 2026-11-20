// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
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
    private final NonNullObservableSupplier<List<TabModel>> mTabModelListSupplier;
    private final Callback<List<TabModel>> mOnTabModelsChanged = this::onTabModelsChanged;

    /** The models currently observed, in the order they were attached. */
    private final List<TabModel> mAttachedTabModels = new ArrayList<>();

    private @Nullable TabModelObserver mTabModelObserver;
    private @Nullable TabGroupObserver mTabGroupObserver;

    /**
     * Constructs a {@link TabListDataProvider}.
     *
     * @param tabModelListSupplier Supplier of the {@link TabModel}s to show tabs from.
     */
    protected TabListDataProvider(NonNullObservableSupplier<List<TabModel>> tabModelListSupplier) {
        mTabModelListSupplier = tabModelListSupplier;
    }

    /**
     * Returns a new supplier of a list holding the current model from {@code tabModelSupplier}, or
     * an empty list when there is none. For surfaces that show tabs from one model.
     *
     * @param tabModelSupplier Supplier of the current {@link TabModel}.
     */
    public static NonNullObservableSupplier<List<TabModel>> createSingleModelListSupplier(
            NullableObservableSupplier<TabModel> tabModelSupplier) {
        return tabModelSupplier.createDerivedNonNull(
                (@Nullable TabModel model) -> model == null ? List.of() : List.of(model));
    }

    /**
     * Adds an observer for data change events.
     *
     * @param observer The {@link TabListDataObserver} to add.
     */
    public void addObserver(TabListDataObserver observer) {
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
     * Attaches to the supplied {@link TabModel}s, rebuilds the item list, and notifies observers
     * via {@link TabListDataObserver#onDataReset}.
     */
    public void requestDataReset() {
        attachToModels(mTabModelListSupplier.get());
        rebuildItems();
        List<TabListItem> unmodifiableItems = Collections.unmodifiableList(mItems);
        notifyObservers(obs -> obs.onDataReset(unmodifiableItems));
    }

    /**
     * Detaches from every attached {@link TabModel} and clears cached items when the surface hides.
     */
    public void stop() {
        detachFromAttachedModels();
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mObservers.clear();
        mTabModelListSupplier.removeObserver(mOnTabModelsChanged);
        detachFromAttachedModels();
        mTabModelObserver = null;
        mTabGroupObserver = null;
    }

    /**
     * Registers observers and starts observing changes to the supplied {@link TabModel}s. Must be
     * called at the end of the subclass constructor after subclass fields are initialized.
     *
     * @param tabModelObserver The {@link TabModelObserver} to register on each attached {@link
     *     TabModel}.
     * @param tabGroupObserver The {@link TabGroupObserver} to register on each attached {@link
     *     TabModel}.
     */
    protected void initObservers(
            TabModelObserver tabModelObserver, TabGroupObserver tabGroupObserver) {
        mTabModelObserver = tabModelObserver;
        mTabGroupObserver = tabGroupObserver;
        assertModelsSupported(mTabModelListSupplier.get());
        mTabModelListSupplier.addSyncObserver(mOnTabModelsChanged);
    }

    /** Returns the first observed {@link TabModel} if its tab state is initialized, or null. */
    protected @Nullable TabModel getTabModelIfTabStateInitialized() {
        // TODO(crbug.com/587126481): Limit this to single-model paths once the highlight model is
        // decided; multi-model paths should use the tab's own model.
        if (mAttachedTabModels.isEmpty()) return null;
        TabModel tabModel = mAttachedTabModels.get(0);
        return tabModel.isTabStateInitialized() ? tabModel : null;
    }

    /** Returns a read-only view of the observed models, in the order they were attached. */
    protected List<TabModel> getAttachedTabModels() {
        return Collections.unmodifiableList(mAttachedTabModels);
    }

    /**
     * Returns the tab with {@code tabId} from the observed models, checking them in order and
     * skipping models whose tab state isn't initialized yet, or null if none has it.
     *
     * @param tabId The ID of the tab to find.
     */
    public @Nullable Tab getTabById(@TabId int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return null;
        for (TabModel model : mAttachedTabModels) {
            if (!model.isTabStateInitialized()) continue;
            Tab tab = model.getTabById(tabId);
            if (tab != null) return tab;
        }
        return null;
    }

    /** Returns the tab to highlight in the list, or null if there is none. */
    protected @Nullable Tab getSelectedTab() {
        // TODO(crbug.com/587126481): The highlight comes from the first model for now; let the host
        // choose which model drives it, or none.
        TabModel model = getTabModelIfTabStateInitialized();
        return model == null ? null : TabModelUtils.getCurrentTab(model);
    }

    /** Rebuilds {@link #mItems} from the attached {@link TabModel}s. */
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
     * Returns whether this provider can show tabs from more than one {@link TabModel}. Subclasses
     * that define an order across models override this.
     */
    protected boolean supportsMultipleModels() {
        return false;
    }

    /**
     * Returns whether the {@link TabGroupItem} for {@code tabGroupId} is about to be removed, so
     * insertions skip past it. Subclasses that show {@link TabGroupItem}s override this.
     *
     * @param tabGroupId The ID of the tab group to check.
     */
    protected boolean isGroupRemoving(Token tabGroupId) {
        return false;
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
     * Creates a {@link TabItem} from the given {@link Tab}.
     *
     * @param selectedTab The tab to highlight, from {@link #getSelectedTab()}, or null if none.
     * @param tab The {@link Tab} to convert.
     * @return A new {@link TabItem} representing {@code tab}.
     */
    protected TabItem createTabItem(@Nullable Tab selectedTab, Tab tab) {
        return new TabItem(
                tab.getId(),
                tab.equals(selectedTab),
                tab.getIsPinned(),
                isTabMultiSelected(tab.getId()));
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

        // Ignore tabs that aren't in a loaded model, such as tabs a model is still restoring.
        TabModel model = getTabModelForTab(tab);
        if (model == null) return;

        insertItem(model, createTabItem(getSelectedTab(), tab));
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
        // Go through moveTabItem so subclasses that keep their own order (e.g. Flat with a
        // comparator) can leave the card in place.
        moveTabItem(tab);
        return existingItem;
    }

    /**
     * Removes the {@link TabItem} for {@code tabId} from {@link #mItems} and notifies observers via
     * {@link TabListDataObserver#onItemsRemoved}.
     *
     * @param tabId The ID of the tab to remove.
     */
    protected void removeTabItem(@TabId int tabId) {
        removeItemsAt(indexOfTabId(tabId), /* count= */ 1);
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

        TabModel model = getTabModelForTab(tab);
        if (model == null) return;

        moveItemsAt(model, indexOfTabId(tab.getId()), /* count= */ 1);
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
        if (getTabModelForTab(tab) == null) return;

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

        // Only TabItems carry pin state: pinned tabs are never grouped, and indexOfTabId only
        // matches TabItems.
        TabItem tabItem = (TabItem) mItems.get(index);
        boolean isPinned = tab.getIsPinned();
        if (tabItem.isPinned() != isPinned) {
            updateItemAt(index, tabItem.withPinned(isPinned), PayloadType.PIN_STATE);
        }
    }

    /**
     * Updates the multi-selected state of all {@link TabItem}s in {@link #mItems} when the set of
     * multi-selected tabs changes in {@link TabModel}.
     */
    protected void updateMultiSelection() {
        for (int i = 0; i < mItems.size(); i++) {
            if (mItems.get(i) instanceof TabItem tabItem) {
                boolean isMultiSelected = isTabMultiSelected(tabItem.getTabId());
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
     * @param model The loaded {@link TabModel} that holds the moved items.
     * @param startIndex The starting index in {@link #mItems}.
     * @param count The number of contiguous items to move.
     */
    protected void moveItemsAt(TabModel model, int startIndex, int count) {
        if (count <= 0 || !isValidIndex(startIndex) || startIndex + count > mItems.size()) {
            return;
        }

        TabListItem targetItem = mItems.get(startIndex);
        List<TabListItem> subList = mItems.subList(startIndex, startIndex + count);
        List<TabListItem> movedItems = List.copyOf(subList);
        subList.clear();

        int toIndex = getInsertionIndex(model, targetItem);
        int insertIndex = toIndex != TabList.INVALID_TAB_INDEX ? toIndex : startIndex;
        mItems.addAll(insertIndex, movedItems);

        // Skip observer notification if the items were restored at startIndex or did not move.
        if (insertIndex == startIndex) return;

        TabListItem previousItem = getPreviousItem(insertIndex);
        notifyObservers(obs -> obs.onItemsMoved(movedItems, previousItem));
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
     * Returns the index in {@link #mItems} of the {@link TabGroupItem} for {@code tabGroupId}, or
     * {@link TabList#INVALID_TAB_INDEX} if absent.
     *
     * @param tabGroupId The ID of the tab group to find.
     */
    protected int indexOfTabGroupId(Token tabGroupId) {
        for (int i = 0; i < mItems.size(); i++) {
            if (matchesTabGroupId(mItems.get(i), tabGroupId)) return i;
        }
        return TabList.INVALID_TAB_INDEX;
    }

    private int indexOfTabId(@TabId int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return TabList.INVALID_TAB_INDEX;
        for (int i = 0; i < mItems.size(); i++) {
            if (matchesTabId(mItems.get(i), tabId)) return i;
        }
        return TabList.INVALID_TAB_INDEX;
    }

    /**
     * Returns the index in {@link #mItems} where {@code targetItem} belongs, based on {@code
     * model}'s tab order, or {@link TabList#INVALID_TAB_INDEX} if it has no position. Subclasses
     * can override to use a different order.
     *
     * @param model The {@link TabModel} whose tab order positions the item.
     * @param targetItem The item to position. It is not in {@link #mItems} when this is called.
     */
    protected int getInsertionIndex(TabModel model, TabListItem targetItem) {
        int itemIndex = 0;
        for (Tab tab : model) {
            int tabId = tab.getId();
            Token tabGroupId = tab.getTabGroupId();
            // Skip headers of groups being removed. When a group's last tab merges into a later
            // group, its empty header stays in mItems until didRemoveTabGroup, and no TabModel
            // tab would ever step past it.
            while (itemIndex < mItems.size()
                    && mItems.get(itemIndex) instanceof TabGroupItem groupItem
                    && isGroupRemoving(groupItem.getTabGroupId())) {
                itemIndex++;
            }
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

    /** Returns whether {@code index} is a valid index in {@link #mItems}. */
    private boolean isValidIndex(int index) {
        return index >= 0 && index < mItems.size();
    }

    /** Returns whether {@code index} is a valid insertion position in {@link #mItems}. */
    private boolean isValidInsertionIndex(int index) {
        return isValidIndex(index) || index == mItems.size();
    }

    private static boolean matchesTabId(TabListItem item, @TabId int tabId) {
        return item instanceof TabItem tabItem && tabItem.getTabId() == tabId;
    }

    /**
     * Returns whether {@code item} is a {@link TabGroupItem} matching {@code tabGroupId}.
     *
     * @param item The {@link TabListItem} to check.
     * @param tabGroupId The tab group {@link Token} to match against, or null.
     */
    private static boolean matchesTabGroupId(TabListItem item, @Nullable Token tabGroupId) {
        return tabGroupId != null
                && item instanceof TabGroupItem groupItem
                && groupItem.getTabGroupId().equals(tabGroupId);
    }

    /** Adds a card for each of {@code model}'s tabs, if the model has loaded. */
    private void addTabItems(TabModel model) {
        if (!model.isTabStateInitialized()) return;
        for (Tab tab : model) {
            addTabItem(tab);
        }
    }

    private void updateSelection(int index, boolean isSelected) {
        TabListItem currentItem = mItems.get(index);
        if (currentItem.isSelected() == isSelected) return;

        updateItemAt(index, currentItem.withSelected(isSelected), PayloadType.SELECTION);
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

    private void onTabModelsChanged(List<TabModel> newModels) {
        assertModelsSupported(newModels);
        if (hasSameModels(newModels)) return;
        if (!Collections.disjoint(mAttachedTabModels, newModels)) {
            // Detach only the models that left and remove their cards, then attach the models that
            // joined and add their tabs.
            detachRemovedModels(newModels);
            attachAddedModels(newModels);
            return;
        }
        // Detach from the previous models on switch; the surface re-attaches on its next
        // requestDataReset().
        detachFromAttachedModels();
    }

    /**
     * Detaches the attached models missing from {@code newModels}, then removes the cards whose tab
     * no remaining model holds.
     */
    private void detachRemovedModels(List<TabModel> newModels) {
        List<TabModel> removedModels = new ArrayList<>(mAttachedTabModels);
        removedModels.removeAll(newModels);
        if (removedModels.isEmpty()) return;
        for (TabModel model : removedModels) {
            detachFromModel(model);
        }
        // Walk backwards so removing a card doesn't shift the ones still to check.
        for (int i = mItems.size() - 1; i >= 0; i--) {
            if (mItems.get(i) instanceof TabItem tabItem
                    && getTabById(tabItem.getTabId()) == null) {
                removeItemsAt(i, /* count= */ 1);
            }
        }
    }

    /** Attaches the models in {@code newModels} that aren't attached yet and adds their tabs. */
    private void attachAddedModels(List<TabModel> newModels) {
        List<TabModel> addedModels = new ArrayList<>(newModels);
        addedModels.removeAll(mAttachedTabModels);
        for (TabModel model : addedModels) {
            attachToModel(model);
            addTabItems(model);
        }
    }

    private void attachToModels(List<TabModel> models) {
        if (hasSameModels(models) || mTabModelObserver == null) return;
        detachFromAttachedModels();
        for (TabModel model : models) {
            attachToModel(model);
        }
    }

    private void detachFromAttachedModels() {
        // detachFromModel() removes from mAttachedTabModels, so loop over a copy.
        for (TabModel model : new ArrayList<>(mAttachedTabModels)) {
            detachFromModel(model);
        }
        mItems.clear();
    }

    private void attachToModel(TabModel model) {
        if (mTabModelObserver != null) {
            model.addObserver(mTabModelObserver);
        }
        if (mTabGroupObserver != null) {
            model.addTabGroupObserver(mTabGroupObserver);
        }
        mAttachedTabModels.add(model);
    }

    private void detachFromModel(TabModel model) {
        if (mTabModelObserver != null) {
            model.removeObserver(mTabModelObserver);
        }
        if (mTabGroupObserver != null) {
            model.removeTabGroupObserver(mTabGroupObserver);
        }
        mAttachedTabModels.remove(model);
    }

    /** Returns the loaded model that holds a tab with {@code tabId}, or null if none does. */
    private @Nullable TabModel getTabModelForTabId(@TabId int tabId) {
        for (TabModel model : mAttachedTabModels) {
            if (model.isTabStateInitialized() && model.getTabById(tabId) != null) return model;
        }
        return null;
    }

    /**
     * Returns the loaded model that holds {@code tab}, or null if none does.
     *
     * @param tab The {@link Tab} to find the model for.
     */
    private @Nullable TabModel getTabModelForTab(Tab tab) {
        return getTabModelForTabId(tab.getId());
    }

    /** Returns whether the loaded model that holds {@code tabId} has it multi-selected. */
    private boolean isTabMultiSelected(@TabId int tabId) {
        // TODO(crbug.com/587126481): A model reports its own current tab as multi-selected, so a
        // background model's current tab counts too. Settle this with the highlight model.
        TabModel model = getTabModelForTabId(tabId);
        return model != null && model.isTabMultiSelected(tabId);
    }

    /** Returns whether {@code models} holds the attached models, in any order. */
    private boolean hasSameModels(List<TabModel> models) {
        return mAttachedTabModels.size() == models.size() && mAttachedTabModels.containsAll(models);
    }

    /** Asserts that this provider can show {@code models} together. */
    private void assertModelsSupported(List<TabModel> models) {
        // Items are placed by walking one model's tab order, which gives no order across models.
        // Showing several models needs a subclass that defines one (e.g. a comparator).
        assert models.size() <= 1 || supportsMultipleModels()
                : "Showing several TabModels needs an order across them, e.g. a comparator.";
        assert haveSameProfile(models) : "All TabModels in one list must share one Profile.";
        assert hasDistinctModels(models) : "All TabModels in one list must be distinct.";
    }

    /**
     * Returns whether every model in {@code models} has the same off-the-record state, and every
     * model that has a profile has the same one.
     */
    private static boolean haveSameProfile(List<TabModel> models) {
        if (models.isEmpty()) return true;
        // An incognito model has no profile until its first tab is created, but it already reports
        // that it's off the record, so compare that for every model and skip the null profile.
        boolean offTheRecord = models.get(0).isOffTheRecord();
        Profile profile = null;
        for (TabModel model : models) {
            if (model.isOffTheRecord() != offTheRecord) return false;
            Profile modelProfile = model.getProfile();
            if (modelProfile == null) continue;
            if (profile == null) {
                profile = modelProfile;
            } else if (modelProfile != profile) {
                return false;
            }
        }
        return true;
    }

    /** Returns whether every model in {@code models} appears only once. */
    private static boolean hasDistinctModels(List<TabModel> models) {
        for (int i = 0; i < models.size(); i++) {
            TabModel model = models.get(i);
            for (int j = i + 1; j < models.size(); j++) {
                if (model.equals(models.get(j))) return false;
            }
        }
        return true;
    }

    List<TabListItem> getItemsForTesting() {
        return Collections.unmodifiableList(mItems);
    }
}
