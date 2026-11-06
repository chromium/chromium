// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.util.SparseArray;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Manages observers for the tab list (e.g. {@link TabGroupObserver}), coordinating their lifecycles
 * and registering them on {@link TabModel} and individual {@link
 * org.chromium.chrome.browser.tab.Tab}s.
 */
@NullMarked
class TabListObserverManager {
    private final TabListLayoutDelegate mLayoutDelegate;
    private final SparseArray<Tab> mObservedTabs = new SparseArray<>();
    private @Nullable TabModel mObservedTabModel;

    /**
     * @param layoutDelegate Delegate handling list structural layout updates and tab group events.
     */
    TabListObserverManager(TabListLayoutDelegate layoutDelegate) {
        mLayoutDelegate = layoutDelegate;
    }

    /**
     * Attaches tab group observation to the given {@link TabModel}.
     *
     * @param tabModel The {@link TabModel} to observe.
     */
    void addTabGroupObserver(TabModel tabModel) {
        mObservedTabModel = tabModel;
        tabModel.addTabGroupObserver(mLayoutDelegate);
    }

    /**
     * Detaches tab group observation from the given {@link TabModel}.
     *
     * @param tabModel The {@link TabModel} to unobserve.
     */
    void removeTabGroupObserver(@Nullable TabModel tabModel) {
        if (tabModel == null) return;
        if (mObservedTabModel == tabModel) {
            mObservedTabModel = null;
        }
        tabModel.removeTabGroupObserver(mLayoutDelegate);
        mLayoutDelegate.reset();
    }

    /**
     * Attaches tab observation to the given {@link Tab}.
     *
     * @param tab The {@link Tab} to observe.
     */
    void addTabObserver(Tab tab) {
        mObservedTabs.put(tab.getId(), tab);
        tab.addObserver(mLayoutDelegate);
    }

    /**
     * Detaches tab observation from the given {@link Tab}.
     *
     * @param tab The {@link Tab} to unobserve.
     */
    void removeTabObserver(@Nullable Tab tab) {
        if (tab == null) return;
        mObservedTabs.remove(tab.getId());
        tab.removeObserver(mLayoutDelegate);
    }

    /**
     * Returns the observed {@link Tab} with the given id. The tab may already be gone from the
     * {@link TabModel}, so it is looked up in the observed tabs instead.
     *
     * @param tabId The id of the tab to find.
     */
    @Nullable Tab getObservedTabById(@TabId int tabId) {
        return mObservedTabs.get(tabId);
    }

    /**
     * Returns the observed {@link Tab}s currently in the given tab group. Returns a copy so callers
     * can detach tabs while iterating.
     *
     * @param tabGroupId The tab group id to match.
     */
    List<Tab> getObservedTabsInGroup(Token tabGroupId) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mObservedTabs.size(); i++) {
            Tab tab = mObservedTabs.valueAt(i);
            if (tabGroupId.equals(tab.getTabGroupId())) tabs.add(tab);
        }
        return tabs;
    }

    /** Unregisters all active observers. */
    void destroy() {
        for (int i = 0; i < mObservedTabs.size(); i++) {
            mObservedTabs.valueAt(i).removeObserver(mLayoutDelegate);
        }
        mObservedTabs.clear();

        if (mObservedTabModel != null) {
            mObservedTabModel.removeTabGroupObserver(mLayoutDelegate);
            mObservedTabModel = null;
        }
        mLayoutDelegate.reset();
    }
}
