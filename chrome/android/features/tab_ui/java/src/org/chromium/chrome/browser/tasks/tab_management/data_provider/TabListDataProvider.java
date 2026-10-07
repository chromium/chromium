// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

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
    }

    /** Rebuilds {@link #mItems} from the current {@link TabModel}. */
    protected abstract void rebuildItems();

    /**
     * Registers the {@link TabModelObserver} and starts observing {@link TabModel} switches. Must
     * be called at the end of the subclass constructor after subclass fields are initialized.
     *
     * @param tabModelObserver The {@link TabModelObserver} to register on the active {@link
     *     TabModel}.
     */
    protected void initObservers(TabModelObserver tabModelObserver) {
        mTabModelObserver = tabModelObserver;
        mTabModelSupplier.addSyncObserver(mOnTabModelChanged);
    }

    /** Returns the current {@link TabModel} if it has finished restoring, or null otherwise. */
    protected @Nullable TabModel getTabModelIfRestored() {
        return mAttachedTabModel != null && mAttachedTabModel.isTabModelRestored()
                ? mAttachedTabModel
                : null;
    }

    /**
     * Creates a {@link TabItem} from the given {@link Tab}.
     *
     * @param selectedTab The currently selected {@link Tab} in the model, or null if none.
     * @param tab The {@link Tab} to convert.
     * @return A new {@link TabItem} representing {@code tab}.
     */
    protected TabItem createTabItem(@Nullable Tab selectedTab, Tab tab) {
        return new TabItem(
                tab.getId(),
                tab.equals(selectedTab),
                tab.getIsPinned(),
                /* isMultiSelected= */ false);
    }

    /**
     * Dispatches {@code callback} to all registered {@link TabListDataObserver}s.
     *
     * @param callback The callback to invoke on each observer.
     */
    protected void notifyObservers(Callback<TabListDataObserver> callback) {
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
        }
    }

    private void detachFromCurrentModel() {
        if (mAttachedTabModel != null && mTabModelObserver != null) {
            mAttachedTabModel.removeObserver(mTabModelObserver);
        }
        mAttachedTabModel = null;
        mItems.clear();
    }

    List<TabListItem> getItemsForTesting() {
        return Collections.unmodifiableList(mItems);
    }
}
