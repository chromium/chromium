// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import androidx.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import javax.inject.Inject;

/**
 * Adds and removes the given {@link PageLoadMetrics.Observer}s and {@link TabObserver}s to Tabs as
 * they enter/leave the TabModel. These managed TabObservers will listen to Tab lifecycle events for
 * *all* tabs in the tab model. </br>
 *
 * <p>This class also provides a different type of TabObserver, {@link CustomTabTabObserver}.
 * Different than the regular managed {@link TabObserver}, this new type of observer will only
 * attach to the current active tab.
 */
@ActivityScope
public class TabObserverRegistrar implements TabModelObserver, DestroyObserver {
    private CustomTabActivityTabProvider mTabProvider;
    private final Set<PageLoadMetrics.Observer> mPageLoadMetricsObservers = new HashSet<>();
    private final Set<TabObserver> mTabObservers = new HashSet<>();

    /** Observers for active tab. */
    private final ObserverList<CustomTabTabObserver> mActivityTabObservers = new ObserverList<>();

    /**
     * Caches the {@link CustomTabActivityTabProvider}'s active tab so that TabObservers can be
     * removed from the previous active tab when the active tab changes.
     */
    private Tab mTabProviderTab;

    private final CustomTabActivityTabProvider.Observer mActivityTabProviderObserver =
            new CustomTabActivityTabProvider.Observer() {
                @Override
                public void onInitialTabCreated(@NonNull Tab tab, @TabCreationMode int mode) {
                    onTabProviderTabUpdated();
                    for (CustomTabTabObserver observer : mActivityTabObservers) {
                        observer.onAttachedToInitialTab(tab);
                    }
                }

                @Override
                public void onTabSwapped(@NonNull Tab tab) {
                    onTabProviderTabUpdated();
                    for (CustomTabTabObserver observer : mActivityTabObservers) {
                        observer.onObservingDifferentTab(tab);
                    }
                }

                @Override
                public void onAllTabsClosed() {
                    onTabProviderTabUpdated();
                    for (CustomTabTabObserver observer : mActivityTabObservers) {
                        observer.onAllTabsClosed();
                    }
                }
            };

    /** Registers a {@link PageLoadMetrics.Observer} to be managed by this Registrar. */
    public void registerPageLoadMetricsObserver(PageLoadMetrics.Observer observer) {
        mPageLoadMetricsObservers.add(observer);
    }

    /**
     * Registers a {@link TabObserver} to be managed by this Registrar. It will receive signals from
     * all tabs in the current tab model.
     *
     * To observe only the active tab, use {@link #registerActivityTabObserver}.
     */
    public void registerTabObserver(TabObserver observer) {
        mTabObservers.add(observer);
    }

    /**
     * Unregisters a {@link TabObserver} to be managed by this Registrar. It will stop receiving
     * signals from any tabs in the current tab model.
     */
    public void unregisterTabObserver(TabObserver observer) {
        mTabObservers.remove(observer);
    }

    /**
     * Registers a TabObserver for the CustomTabActivity's active tab. Changes the Tab that is
     * being observed when the CustomTabActivity's active tab changes.
     * Differs from {@link #registerTabObserver} which observes all newly created tabs.
     */
    public void registerActivityTabObserver(CustomTabTabObserver observer) {
        mActivityTabObservers.addObserver(observer);
        Tab activeTab = mTabProvider.getTab();
        if (activeTab != null) {
            activeTab.addObserver(observer);
            observer.onAttachedToInitialTab(activeTab);
        }
    }

    public void unregisterActivityTabObserver(CustomTabTabObserver observer) {
        mActivityTabObservers.removeObserver(observer);
        Tab activeTab = mTabProvider.getTab();
        if (activeTab != null) {
            activeTab.removeObserver(observer);
        }
    }

    @Inject
    public TabObserverRegistrar(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabActivityTabProvider tabProvider) {
        mTabProvider = tabProvider;
        mTabProvider.addObserver(mActivityTabProviderObserver);

        lifecycleDispatcher.register(this);
    }

    @Override
    public void didAddTab(
            Tab tab, int type, @TabCreationState int creationState, boolean markedForSelection) {
        addObserversForTab(tab);
    }

    @Override
    public void onFinishingTabClosure(Tab tab) {
        // We don't need to remove the Tab Observers since it's closed.
        // TODO(peconn): Do we really want to remove the *global* PageLoadMetrics observers here?
        removePageLoadMetricsObservers();
    }

    @Override
    public void tabRemoved(Tab tab) {
        removePageLoadMetricsObservers();
        removeTabObservers(tab, mTabObservers.iterator());
    }

    /**
     * Adds all currently registered {@link PageLoadMetrics.Observer}s and {@link TabObserver}s to
     * the global {@link PageLoadMetrics} object and the given {@link Tab} respectively.
     */
    public void addObserversForTab(Tab tab) {
        addPageLoadMetricsObservers();
        addTabObservers(tab, mTabObservers.iterator());
    }

    private void addPageLoadMetricsObservers() {
        for (PageLoadMetrics.Observer observer : mPageLoadMetricsObservers) {
            PageLoadMetrics.addObserver(observer, false);
        }
    }

    private void removePageLoadMetricsObservers() {
        for (PageLoadMetrics.Observer observer : mPageLoadMetricsObservers) {
            PageLoadMetrics.removeObserver(observer);
        }
    }

    /** Called when the {@link CustomTabActivityTabProvider}'s active tab has changed. */
    private void onTabProviderTabUpdated() {
        if (mTabProviderTab != null) {
            removeTabObservers(mTabProviderTab, mActivityTabObservers.iterator());
        }
        mTabProviderTab = mTabProvider.getTab();
        if (mTabProviderTab != null) {
            addTabObservers(mTabProviderTab, mActivityTabObservers.iterator());
        }
    }

    private void addTabObservers(Tab tab, Iterator<? extends TabObserver> tabObserversIterator) {
        while (tabObserversIterator.hasNext()) {
            tab.addObserver(tabObserversIterator.next());
        }
    }

    private void removeTabObservers(Tab tab, Iterator<? extends TabObserver> tabObserversIterator) {
        while (tabObserversIterator.hasNext()) {
            tab.removeObserver(tabObserversIterator.next());
        }
    }

    @Override
    public void onDestroy() {
        removePageLoadMetricsObservers();
    }

    /**
     * A class for observing the activity tab. When the activity tab changes, the observer is
     * switched to that tab.
     */
    public abstract static class CustomTabTabObserver extends EmptyTabObserver {
        /**
         * Called when the initial tab is created or the observer is registered with
         * {@link TabObserverRegistrar}, whichever occurs last.
         */
        protected void onAttachedToInitialTab(@NonNull Tab tab) {}

        /**
         * A notification that the observer has switched to observing a different tab. This will not
         * be called for the initial tab being attached to after creation.
         * @param tab The tab that the observer is now observing.
         */
        protected void onObservingDifferentTab(@NonNull Tab tab) {}

        /**
         * A notification that the observer has been removed from the tab, as all the tabs are
         * closing and there's no active tabs left. This is useful when observers need to release
         * related dependencies when observers are removed from the tab while the tab is still kept
         * alive (e.g. during tab reparenting).
         */
        protected void onAllTabsClosed() {}
    }
}
