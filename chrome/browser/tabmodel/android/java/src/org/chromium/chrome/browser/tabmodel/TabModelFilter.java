// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * This class is responsible for filtering tabs from {@link TabModel}. The filtering logic is
 * delegated to the concrete class that extends this abstract class. If no filter is active, this
 * class has the same {@link TabList} as {@link TabModel} does.
 *
 * If there is at least one filter active, this is a {@link TabList} that contains the most
 * important tabs that the filter defines.
 */
public abstract class TabModelFilter implements TabModelObserver, TabList {
    private static final List<Tab> sEmptyRelatedTabList =
            Collections.unmodifiableList(new ArrayList<Tab>());
    private static final List<Integer> sEmptyRelatedTabIds =
            Collections.unmodifiableList(new ArrayList<Integer>());
    private TabModel mTabModel;
    protected ObserverList<TabModelObserver> mFilteredObservers = new ObserverList<>();
    private boolean mTabRestoreCompleted;
    private boolean mTabStateInitialized;

    public TabModelFilter(TabModel tabModel) {
        mTabModel = tabModel;
        mTabModel.addObserver(this);
    }

    /**
     * Adds a {@link TabModelObserver} to be notified on {@link TabModelFilter} changes.
     * @param observer The {@link TabModelObserver} to add.
     */
    public void addObserver(TabModelObserver observer) {
        mFilteredObservers.addObserver(observer);
    }

    /**
     * Removes a {@link TabModelObserver}.
     * @param observer The {@link TabModelObserver} to remove.
     */
    public void removeObserver(TabModelObserver observer) {
        mFilteredObservers.removeObserver(observer);
    }

    public boolean isCurrentlySelectedFilter() {
        return getTabModel().isActiveModel();
    }

    /**
     * Mark TabState initialized, and TabModelFilter ready to use. This should only be called once,
     * and should only be called by {@link TabModelFilterProvider}.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void markTabStateInitialized() {
        assert !mTabStateInitialized;
        mTabStateInitialized = true;
    }

    /**
     * To be called when this filter should be destroyed. This filter should no longer be used after
     * this.
     */
    public void destroy() {
        mFilteredObservers.clear();
    }

    /**
     * @return The {@link TabModel} that the filter is acting on.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public TabModel getTabModel() {
        return mTabModel;
    }

    /**
     * @return The total tab count in the underlying {@link TabModel}.
     */
    public int getTotalTabCount() {
        return mTabModel.getCount();
    }

    /**
     * Any of the concrete class can override and define a relationship that links a {@link Tab} to
     * a list of related {@link Tab}s. By default, this returns an unmodifiable list that only
     * contains the {@link Tab} with the given id. Note that the meaning of related can vary
     * depending on the filter being applied.
     * @param tabId Id of the {@link Tab} try to relate with.
     * @return An unmodifiable list of {@link Tab} that relate with the given tab id.
     */
    @NonNull
    public List<Tab> getRelatedTabList(int tabId) {
        Tab tab = TabModelUtils.getTabById(getTabModel(), tabId);
        if (tab == null) return sEmptyRelatedTabList;
        List<Tab> relatedTab = new ArrayList<>();
        relatedTab.add(tab);
        return Collections.unmodifiableList(relatedTab);
    }

    /**
     * Any of the concrete class can override and define a relationship that links a {@link Tab} to
     * a list of related {@link Tab}s. By default, this returns an unmodifiable list that only
     * contains the given id. Note that the meaning of related can vary
     * depending on the filter being applied.
     * @param tabId Id of the {@link Tab} try to relate with.
     * @return An unmodifiable list of id that relate with the given tab id.
     */
    @NonNull
    public List<Integer> getRelatedTabIds(int tabId) {
        Tab tab = TabModelUtils.getTabById(getTabModel(), tabId);
        if (tab == null) return sEmptyRelatedTabIds;
        List<Integer> relatedTabIds = new ArrayList<>();
        relatedTabIds.add(tabId);
        return Collections.unmodifiableList(relatedTabIds);
    }

    /**
     * @return An unmodifiable list of {@link Tab}s that are not related to any tabs
     */
    @NonNull
    public final List<Tab> getTabsWithNoOtherRelatedTabs() {
        List<Tab> tabs = new ArrayList<>();
        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            if (!hasOtherRelatedTabs(tab)) {
                tabs.add(tab);
            }
        }
        return Collections.unmodifiableList(tabs);
    }

    /**
     * Any of the concrete class that defined a relationship between tabs should override this
     * method. By default, the given {@link Tab} has no related tabs, other than itself.
     * @param tab A {@link Tab}.
     * @return Whether the given {@link Tab} has other related tabs that is not itself.
     */
    public boolean hasOtherRelatedTabs(Tab tab) {
        return false;
    }

    /**
     * Returns a valid position to add or move a tab to this model in the context of any related
     * tabs.
     * @param tab The tab to be added/moved.
     * @param proposedPosition The current or proposed position of the tab in the model.
     * @return a valid position close to proposedPosition that respects related tab ordering rules.
     */
    public abstract int getValidPosition(Tab tab, int proposedPosition);

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} added a
     * {@link Tab}.
     * @param tab {@link Tab} had added to {@link TabModel}.
     */
    protected abstract void addTab(Tab tab);

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} closed a
     * {@link Tab}.
     * @param tab {@link Tab} had closed from {@link TabModel}.
     */
    protected abstract void closeTab(Tab tab);

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} selected a
     * {@link Tab}.
     * @param tab {@link Tab} had selected.
     */
    protected abstract void selectTab(Tab tab);

    /**
     * Concrete class requires to define the ordering of each Tab within the filter.
     */
    protected abstract void reorder();

    /**
     * Concrete class requires to define what to clean up.
     */
    protected abstract void resetFilterStateInternal();

    /**
     * @return Whether the tab model is fully restored.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean isTabModelRestored() {
        // TODO(crbug.com/1081339): Remove |mTabRestoreCompleted|. |mTabRestoreCompleted| is always
        // false for incognito, while |mTabStateInitialized| is not. |mTabStateInitialized| is
        // marked after the TabModelSelector is initialized, therefore it is the true state of the
        // TabModel.
        return mTabRestoreCompleted || mTabStateInitialized;
    }

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} removed a
     * {@link Tab}.
     * @param tab {@link Tab} had removed.
     */
    protected abstract void removeTab(Tab tab);

    /**
     * Calls {@code resetFilterStateInternal} method to clean up filter internal data, and resets
     * the internal data based on the current {@link TabModel}.
     */
    protected void resetFilterState() {
        resetFilterStateInternal();

        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            addTab(tab);
        }
    }

    // TODO(crbug.com/948518): This is a band-aid fix for not crashing when undo the last closed
    // tab, should remove later.
    /**
     * @return Whether filter should notify observers about the SetIndex call.
     */
    protected boolean shouldNotifyObserversOnSetIndex() {
        return true;
    }

    // TabModelObserver implementation.
    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        selectTab(tab);
        if (!shouldNotifyObserversOnSetIndex()) return;
        for (TabModelObserver observer : mFilteredObservers) {
            observer.didSelectTab(tab, type, lastId);
        }
    }

    @Override
    public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
        closeTab(tab);
        for (TabModelObserver observer : mFilteredObservers) {
            observer.willCloseTab(tab, animate, didCloseAlone);
        }
    }

    @Override
    public void onFinishingTabClosure(Tab tab) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.onFinishingTabClosure(tab);
        }
    }

    @Override
    public void onFinishingMultipleTabClosure(List<Tab> tabs) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.onFinishingMultipleTabClosure(tabs);
        }
    }

    @Override
    public void willAddTab(Tab tab, int type) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.willAddTab(tab, type);
        }
    }

    @Override
    public void didAddTab(Tab tab, @TabLaunchType int type, @TabCreationState int creationState,
            boolean markedForSelection) {
        addTab(tab);
        for (TabModelObserver observer : mFilteredObservers) {
            observer.didAddTab(tab, type, creationState, markedForSelection);
        }
    }

    @Override
    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.didMoveTab(tab, newIndex, curIndex);
        }
    }

    @Override
    public void tabPendingClosure(Tab tab) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.tabPendingClosure(tab);
        }
    }

    @Override
    public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.multipleTabsPendingClosure(tabs, isAllTabs);
        }
    }

    @Override
    public void tabClosureUndone(Tab tab) {
        addTab(tab);
        reorder();
        for (TabModelObserver observer : mFilteredObservers) {
            observer.tabClosureUndone(tab);
        }
    }

    @Override
    public void tabClosureCommitted(Tab tab) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.tabClosureCommitted(tab);
        }
    }

    @Override
    public void willCloseAllTabs(boolean incognito) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.willCloseAllTabs(incognito);
        }
    }

    @Override
    public void allTabsClosureUndone() {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.allTabsClosureUndone();
        }
    }

    @Override
    public void allTabsClosureCommitted(boolean isIncognito) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.allTabsClosureCommitted(isIncognito);
        }
    }

    @Override
    public void tabRemoved(Tab tab) {
        removeTab(tab);
        for (TabModelObserver observer : mFilteredObservers) {
            observer.tabRemoved(tab);
        }
    }

    @Override
    public void restoreCompleted() {
        mTabRestoreCompleted = true;

        if (getCount() != 0) reorder();

        for (TabModelObserver observer : mFilteredObservers) {
            observer.restoreCompleted();
        }
    }
}
