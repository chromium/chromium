// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;

import java.util.ArrayList;
import java.util.List;

/**
 * AllTabObserver observes all tab additions and removals (for every window).
 *
 * <p>AllTabObserver will initially notify for all current tabs when constructed.
 */
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

    /** Observes all added and removed tabs. */
    public AllTabObserver(Observer observer) {
        mTabWindowManager = TabWindowManagerSingleton.getInstance();
        mObserver = observer;
        for (int i = 0; i < mTabWindowManager.getMaxSimultaneousSelectors(); i++) {
            var selector = mTabWindowManager.getTabModelSelectorById(i);
            if (selector != null) {
                mTabModelSelectorStates.add(new TabModelSelectorState(selector));
            }
        }

        mTabWindowManager.addObserver(this);
    }

    /** Destroys this object. */
    public void destroy() {
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
}
