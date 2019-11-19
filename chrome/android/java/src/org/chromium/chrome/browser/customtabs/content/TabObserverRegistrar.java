// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;

import java.util.HashSet;
import java.util.Set;

import javax.inject.Inject;

/**
 * Adds and removes the given {@link PageLoadMetrics.Observer}s and {@link TabObserver}s to Tabs as
 * they enter/leave the TabModel.
 *
 * // TODO(peconn): Get rid of EmptyTabModelObserver now that we have Java 8 default methods.
 */
@ActivityScope
public class TabObserverRegistrar extends EmptyTabModelObserver implements Destroyable {
    private CustomTabActivityTabProvider mTabProvider;
    private final Set<PageLoadMetrics.Observer> mPageLoadMetricsObservers = new HashSet<>();
    private final Set<TabObserver> mTabObservers = new HashSet<>();

    /** Observers for active tab. */
    private final Set<TabObserver> mActivityTabObservers = new HashSet<>();

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
                }

                @Override
                public void onTabSwapped(@NonNull Tab tab) {
                    onTabProviderTabUpdated();
                }

                @Override
                public void onAllTabsClosed() {
                    onTabProviderTabUpdated();
                }
            };

    /**
     * Registers a {@link PageLoadMetrics.Observer} to be managed by this Registrar.
     */
    public void registerPageLoadMetricsObserver(PageLoadMetrics.Observer observer) {
        mPageLoadMetricsObservers.add(observer);
    }

    /**
     * Registers a {@link TabObserver} to be managed by this Registrar.
     */
    public void registerTabObserver(TabObserver observer) {
        mTabObservers.add(observer);
    }

    /**
     * Unregisters a {@link TabObserver} to be managed by this Registrar.
     */
    public void unregisterTabObserver(TabObserver observer) {
        mTabObservers.remove(observer);
    }

    /**
     * Registers a TabObserver for the CustomTabActivity's active tab. Changes the Tab that is
     * being observed when the CustomTabActivity's active tab changes.
     * Differs from {@link #registerTabObserver()} which observes all newly created tabs.
     */
    public void registerActivityTabObserver(TabObserver observer) {
        mActivityTabObservers.add(observer);
        Tab activeTab = mTabProvider.getTab();
        if (activeTab != null) {
            activeTab.addObserver(observer);
        }
    }

    public void unregisterActivityTabObserver(TabObserver observer) {
        mActivityTabObservers.remove(observer);
        Tab activeTab = mTabProvider.getTab();
        if (activeTab != null) {
            activeTab.removeObserver(observer);
        }
    }

    @Inject
    public TabObserverRegistrar(ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabActivityTabProvider tabProvider) {
        mTabProvider = tabProvider;
        mTabProvider.addObserver(mActivityTabProviderObserver);

        lifecycleDispatcher.register(this);
    }

    @Override
    public void didAddTab(Tab tab, int type) {
        addObserversForTab(tab);
    }

    @Override
    public void didCloseTab(int tabId, boolean incognito) {
        // We don't need to remove the Tab Observers since it's closed.
        // TODO(peconn): Do we really want to remove the *global* PageLoadMetrics observers here?
        removePageLoadMetricsObservers();
    }

    @Override
    public void tabRemoved(Tab tab) {
        removePageLoadMetricsObservers();
        removeTabObservers(tab, mTabObservers);
    }

    /**
     * Adds all currently registered {@link PageLoadMetrics.Observer}s and {@link TabObserver}s to
     * the global {@link PageLoadMetrics} object and the given {@link Tab} respectively.
     */
    public void addObserversForTab(Tab tab) {
        addPageLoadMetricsObservers();
        addTabObservers(tab, mTabObservers);
    }

    private void addPageLoadMetricsObservers() {
        for (PageLoadMetrics.Observer observer : mPageLoadMetricsObservers) {
            PageLoadMetrics.addObserver(observer);
        }
    }

    private void removePageLoadMetricsObservers() {
        for (PageLoadMetrics.Observer observer : mPageLoadMetricsObservers) {
            PageLoadMetrics.removeObserver(observer);
        }
    }

    /**
     * Called when the {@link CustomTabActivityTabProvider}'s active tab has changed.
     */
    private void onTabProviderTabUpdated() {
        if (mTabProviderTab != null) {
            removeTabObservers(mTabProviderTab, mActivityTabObservers);
        }
        mTabProviderTab = mTabProvider.getTab();
        if (mTabProviderTab != null) {
            addTabObservers(mTabProviderTab, mActivityTabObservers);
        }
    }

    private void addTabObservers(Tab tab, Set<TabObserver> tabObservers) {
        for (TabObserver observer : tabObservers) {
            tab.addObserver(observer);
        }
    }

    private void removeTabObservers(Tab tab, Set<TabObserver> tabObservers) {
        for (TabObserver observer : tabObservers) {
            tab.removeObserver(observer);
        }
    }

    @Override
    public void destroy() {
        removePageLoadMetricsObservers();
    }
}
