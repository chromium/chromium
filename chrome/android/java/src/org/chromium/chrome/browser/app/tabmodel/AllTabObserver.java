// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.ArrayList;
import java.util.List;

/**
 * AllTabObserver observes all tab additions and removals (for every window).
 *
 * <p>AllTabObserver will initially notify for all current tabs when constructed.
 */
@NullMarked
public class AllTabObserver implements TabWindowManager.Observer, TabModelObserver {
    public interface Observer {
        /** Called when a Tab is added. */
        void onTabAdded(Tab tab);

        /** Called when a Tab is removed. */
        void onTabRemoved(Tab tab);
    }

    private class TabModelSelectorState extends TabModelSelectorTabObserver
            implements TabModelSelectorObserver {
        private final TabModelSelector mTabModelSelector;

        /**
         * Constructs a TabModelSelectorState.
         *
         * @param tabModelSelector TabModelSelector to observe.
         */
        public TabModelSelectorState(TabModelSelector tabModelSelector) {
            super(tabModelSelector);

            mTabModelSelector = tabModelSelector;
            mTabModelSelector.addObserver(this);
        }

        @Override
        public void destroy() {
            // Calling `TabModelSelectorTabObserver.destroy` will update the observers with the tabs
            // that are now gone.
            super.destroy();
            mTabModelSelector.removeObserver(this);
        }

        // TabModelSelectorObserver implementation.
        @Override
        public void onDestroyed() {
            destroy();
            var removed = mTabModelSelectorStates.remove(this);
            assert removed;
        }

        // TabModelSelectorTabObserver implementation.
        @Override
        public void onTabRegistered(Tab tab) {
            mObserver.onTabAdded(tab);
        }

        @Override
        public void onTabUnregistered(Tab tab) {
            mObserver.onTabRemoved(tab);
        }
    }

    private final TabWindowManager mTabWindowManager;
    private final List<TabModelSelectorState> mTabModelSelectorStates = new ArrayList<>();
    private final Observer mObserver;

    private static final List<AllTabObserver> sAllTabObservers = new ArrayList<>();
    private static final List<Tab> sCustomTabs = new ArrayList<>();

    /** Call when a {@link Tab} that is not covered by {@link TabWindowManager} is added. */
    public static void addCustomTab(Tab tab) {
        sCustomTabs.add(tab);
        for (var allTabObserver : sAllTabObservers) {
            allTabObserver.mObserver.onTabAdded(tab);
        }
    }

    /** Call when a {@link Tab} that is not covered by {@link TabWindowManager} is removed. */
    public static void removeCustomTab(Tab tab) {
        if (sCustomTabs.remove(tab)) {
            for (var allTabObserver : sAllTabObservers) {
                allTabObserver.mObserver.onTabRemoved(tab);
            }
        }
    }

    /** Observes all added and removed tabs. */
    public AllTabObserver(Observer observer) {
        mTabWindowManager = TabWindowManagerSingleton.getInstance();
        mObserver = observer;
        for (TabModelSelector selector : mTabWindowManager.getAllTabModelSelectors()) {
            if (selector != null) {
                mTabModelSelectorStates.add(new TabModelSelectorState(selector));
            }
        }

        mTabWindowManager.addObserver(this);
        sAllTabObservers.add(this);
        for (Tab tab : sCustomTabs) {
            mObserver.onTabAdded(tab);
        }
    }

    /** Destroys this object. */
    public void destroy() {
        sAllTabObservers.remove(this);
        mTabWindowManager.removeObserver(this);
        for (var tabModelSelectorState : mTabModelSelectorStates) {
            tabModelSelectorState.destroy();
        }
        mTabModelSelectorStates.clear();
    }

    // TabWindowManager.Observer implementation.
    @Override
    public void onTabModelSelectorAdded(TabModelSelector selector) {
        mTabModelSelectorStates.add(new TabModelSelectorState(selector));
    }

    public static void resetForTesting() {
        sCustomTabs.clear();
        sAllTabObservers.clear();
    }
}
