// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Abstract base class that is responsible for filtering tabs from {@link TabModel}. The filtering
 * logic is delegated to the concrete class that extends this abstract class.
 */
public abstract class TabModelFilterBase implements TabModelFilter, TabModelObserver {
    private static final List<Tab> sEmptyRelatedTabList =
            Collections.unmodifiableList(new ArrayList<Tab>());
    private static final List<Integer> sEmptyRelatedTabIds =
            Collections.unmodifiableList(new ArrayList<Integer>());
    private TabModel mTabModel;
    protected ObserverList<TabModelObserver> mFilteredObservers = new ObserverList<>();
    private boolean mTabRestoreCompleted;
    private boolean mTabStateInitialized;

    /**
     * @param tabModel The tab model to filter.
     */
    public TabModelFilterBase(TabModel tabModel) {
        mTabModel = tabModel;
        mTabModel.addObserver(this);
    }

    @Override
    public void destroy() {
        mFilteredObservers.clear();
        mTabModel.removeObserver(this);
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mFilteredObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mFilteredObservers.removeObserver(observer);
    }

    @Override
    public boolean isCurrentlySelectedFilter() {
        return getTabModel().isActiveModel();
    }

    @Override
    public @NonNull TabModel getTabModel() {
        return mTabModel;
    }

    @Override
    public int getTotalTabCount() {
        return mTabModel.getCount();
    }

    @Override
    public @NonNull List<Tab> getRelatedTabList(int tabId) {
        Tab tab = getTabModel().getTabById(tabId);
        if (tab == null) return sEmptyRelatedTabList;
        List<Tab> relatedTab = new ArrayList<>();
        relatedTab.add(tab);
        return Collections.unmodifiableList(relatedTab);
    }

    @Override
    public @NonNull List<Integer> getRelatedTabIds(int tabId) {
        Tab tab = getTabModel().getTabById(tabId);
        if (tab == null) return sEmptyRelatedTabIds;
        List<Integer> relatedTabIds = new ArrayList<>();
        relatedTabIds.add(tabId);
        return Collections.unmodifiableList(relatedTabIds);
    }

    @Override
    public boolean isTabInTabGroup(Tab tab) {
        return false;
    }

    @Override
    public boolean isTabModelRestored() {
        // TODO(crbug.com/40130477): Remove |mTabRestoreCompleted|. |mTabRestoreCompleted| is always
        // false for incognito, while |mTabStateInitialized| is not. |mTabStateInitialized| is
        // marked after the TabModelSelector is initialized, therefore it is the true state of the
        // TabModel.
        return mTabRestoreCompleted || mTabStateInitialized;
    }

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} added a {@link
     * Tab}.
     *
     * @param tab {@link Tab} had added to {@link TabModel}.
     * @param fromUndo Whether the tab was added by undo.
     */
    protected abstract void addTab(Tab tab, boolean fromUndo);

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} closed a {@link
     * Tab}.
     *
     * @param tab {@link Tab} had closed from {@link TabModel}.
     */
    protected abstract void closeTab(Tab tab);

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} selected a {@link
     * Tab}.
     *
     * @param tab {@link Tab} had selected.
     */
    protected abstract void selectTab(Tab tab);

    /** Concrete class requires to define the ordering of each Tab within the filter. */
    protected abstract void reorder();

    /** Concrete class requires to define what to clean up. */
    protected abstract void resetFilterStateInternal();

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} removed a {@link
     * Tab}.
     *
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
            addTab(tab, /* fromUndo= */ false);
        }
    }

    // TODO(crbug.com/41450619): This is a band-aid fix for not crashing when undo the last closed
    // tab, should remove later.
    /** Returns whether filter should notify observers about the SetIndex call. */
    protected boolean shouldNotifyObserversOnSetIndex() {
        return true;
    }

    /**
     * Mark TabState initialized, and TabModelFilter ready to use. This should only be called once,
     * and should only be called by {@link TabModelFilterProvider}.
     */
    protected void markTabStateInitialized() {
        assert !mTabStateInitialized;
        mTabStateInitialized = true;
    }

    // TabModelObserver implementation.
    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        RecordHistogram.recordBooleanHistogram(
                "TabGroups.SelectedTabInTabGroup", isTabInTabGroup(tab));
        selectTab(tab);
        if (!shouldNotifyObserversOnSetIndex()) return;
        for (TabModelObserver observer : mFilteredObservers) {
            observer.didSelectTab(tab, type, lastId);
        }
    }

    @Override
    public void willCloseTab(Tab tab, boolean didCloseAlone) {
        closeTab(tab);
        for (TabModelObserver observer : mFilteredObservers) {
            observer.willCloseTab(tab, didCloseAlone);
        }
    }

    @Override
    public void onFinishingTabClosure(Tab tab) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.onFinishingTabClosure(tab);
        }
    }

    @Override
    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.onFinishingMultipleTabClosure(tabs, canRestore);
        }
    }

    @Override
    public void willAddTab(Tab tab, int type) {
        for (TabModelObserver observer : mFilteredObservers) {
            observer.willAddTab(tab, type);
        }
    }

    @Override
    public void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        addTab(tab, /* fromUndo= */ false);
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
        addTab(tab, /* fromUndo= */ true);
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
