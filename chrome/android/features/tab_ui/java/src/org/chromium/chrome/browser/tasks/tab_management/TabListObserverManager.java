// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.HashSet;
import java.util.Set;

/**
 * Manages observers for the tab list (e.g. {@link TabGroupObserver}), coordinating their lifecycles
 * and registering them on {@link TabModel} and individual {@link
 * org.chromium.chrome.browser.tab.Tab}s.
 */
@NullMarked
class TabListObserverManager {
    private final TabListLayoutDelegate mLayoutDelegate;
    private final Set<Tab> mObservedTabs = new HashSet<>();
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
    }

    /**
     * Attaches tab observation to the given {@link Tab}.
     *
     * @param tab The {@link Tab} to observe.
     */
    void addTabObserver(Tab tab) {
        mObservedTabs.add(tab);
        tab.addObserver(mLayoutDelegate);
    }

    /**
     * Detaches tab observation from the given {@link Tab}.
     *
     * @param tab The {@link Tab} to unobserve.
     */
    void removeTabObserver(@Nullable Tab tab) {
        if (tab == null) return;
        mObservedTabs.remove(tab);
        tab.removeObserver(mLayoutDelegate);
    }

    /** Unregisters all active observers. */
    void destroy() {
        for (Tab tab : mObservedTabs) {
            tab.removeObserver(mLayoutDelegate);
        }
        mObservedTabs.clear();

        if (mObservedTabModel != null) {
            mObservedTabModel.removeTabGroupObserver(mLayoutDelegate);
            mObservedTabModel = null;
        }
    }
}
