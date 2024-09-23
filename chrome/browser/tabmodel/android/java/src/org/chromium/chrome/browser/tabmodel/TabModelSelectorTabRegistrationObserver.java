// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.SparseArray;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;

import java.util.LinkedHashSet;
import java.util.List;

/** Allows observing registration (e.g. add / remove) events of Tabs from a TabModelSelector. */
public final class TabModelSelectorTabRegistrationObserver {
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final SparseArray<Tab> mTabsToClose = new SparseArray<>();
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final LinkedHashSet<Tab> mRegisteredTabs = new LinkedHashSet<>();

    private boolean mTabModelSelectorRegistrationComplete;

    /** The Observer that will receive updates about a tabs registration to a tab model. */
    public interface Observer {
        /**
         * Called when a tab is registered to a tab model this selector is managing.
         * @param tab The registered Tab.
         */
        void onTabRegistered(Tab tab);

        /**
         * Called when a tab is unregistered from a tab model this selector is managing.
         * @param tab The unregistered Tab.
         */
        void onTabUnregistered(Tab tab);
    }

    /**
     * Constructs an observer that should be notified of tab changes for all tabs owned
     * by a specified {@link TabModelSelector}.  Any Tabs created after this call will be
     * observed as well, and Tabs removed will no longer have their information broadcast.
     *
     * <p>
     * {@link #destroy()} must be called to unregister this observer.
     *
     * @param selector The selector that owns the Tabs that should notify this observer.
     */
    public TabModelSelectorTabRegistrationObserver(TabModelSelector selector) {
        mTabModelObserver =
                new TabModelSelectorTabModelObserver(selector) {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        // This observer is automatically removed by tab when it is destroyed.
                        onTabRegistered(tab);
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        mTabsToClose.put(tab.getId(), tab);
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        mTabsToClose.remove(tab.getId());
                    }

                    @Override
                    public void onFinishingTabClosure(Tab tab) {
                        if (mTabsToClose.get(tab.getId()) != null) {
                            mTabsToClose.remove(tab.getId());
                            onTabUnregistered(tab);
                        }
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        onTabUnregistered(tab);
                    }

                    @Override
                    protected void onRegistrationComplete() {
                        assert !mTabModelSelectorRegistrationComplete;
                        mTabModelSelectorRegistrationComplete = true;

                        List<TabModel> tabModels = selector.getModels();
                        for (int i = 0; i < tabModels.size(); i++) {
                            TabModel tabModel = tabModels.get(i);
                            TabList comprehensiveTabList = tabModel.getComprehensiveModel();
                            for (int j = 0; j < comprehensiveTabList.getCount(); j++) {
                                onTabRegistered(comprehensiveTabList.getTabAt(j));
                            }
                        }
                    }
                };
    }

    /**
     * Adds an observer for future tab registration events, and notifies the observer of any tabs
     * that have already been registered (synchronously).
     * @param observer The observer to be added.
     */
    public void addObserverAndNotifyExistingTabRegistration(Observer observer) {
        mObservers.addObserver(observer);
        if (mTabModelSelectorRegistrationComplete) {
            for (Tab tab : mRegisteredTabs) observer.onTabRegistered(tab);
        }
    }

    /**
     * Removes an observer for subsequent tab registration events.
     * @param observer The observer to be removed.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Called when a tab is registered to a tab model this selector is managing.
     * @param tab The registered Tab.
     */
    private void onTabRegistered(Tab tab) {
        if (mRegisteredTabs.add(tab)) {
            for (Observer observer : mObservers) observer.onTabRegistered(tab);
        }
    }

    /**
     * Called when a tab is unregistered from a tab model this selector is managing.
     * @param tab The unregistered Tab.
     */
    private void onTabUnregistered(Tab tab) {
        if (mRegisteredTabs.remove(tab)) {
            for (Observer observer : mObservers) observer.onTabUnregistered(tab);
        }
    }

    /** Destroys the observer and removes itself as a listener for Tab updates. */
    public void destroy() {
        mTabModelObserver.destroy();
        Tab[] tabs = mRegisteredTabs.toArray(new Tab[0]);
        for (Tab tab : tabs) onTabUnregistered(tab);
        assert mRegisteredTabs.isEmpty();
    }
}
