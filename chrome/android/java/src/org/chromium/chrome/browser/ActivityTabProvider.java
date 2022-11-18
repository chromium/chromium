// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

/**
 * A class that provides the current {@link Tab} for various states of the browser's activity.
 */
public class ActivityTabProvider extends ObservableSupplierImpl<Tab> implements Destroyable {
    /**
     * A utility class for observing the activity tab via {@link TabObserver}. When the activity
     * tab changes, the observer is switched to that tab.
     */
    public static class ActivityTabTabObserver extends EmptyTabObserver {
        /** A handle to the activity tab provider. */
        private final ActivityTabProvider mTabProvider;

        /** An observer to watch for a changing activity tab and move this tab observer. */
        private final Callback<Tab> mActivityTabObserver;

        /** The current activity tab. */
        private Tab mTab;

        /**
         * Create a new {@link TabObserver} that only observes the activity tab. It doesn't trigger
         * for the initial tab being attached to after creation.
         * @param tabProvider An {@link ActivityTabProvider} to get the activity tab.
         */
        public ActivityTabTabObserver(ActivityTabProvider tabProvider) {
            this(tabProvider, false);
        }

        /**
         * Create a new {@link TabObserver} that only observes the activity tab. This constructor
         * allows the option of triggering for the initial tab being attached to after creation.
         * @param tabProvider An {@link ActivityTabProvider} to get the activity tab.
         * @param shouldTrigger Whether the observer should be triggered for the initial tab after
         * creation.
         */
        public ActivityTabTabObserver(ActivityTabProvider tabProvider, boolean shouldTrigger) {
            mTabProvider = tabProvider;
            mActivityTabObserver = (tab) -> {
                updateObservedTab(tab);
                onObservingDifferentTab(tab, /*hint=*/false);
            };

            addObserverToTabProvider();
            if (shouldTrigger) onObservingDifferentTab(tabProvider.get(), /*hint=*/false);

            updateObservedTabToCurrent();
        }

        /**
         * Update the tab being observed.
         * @param newTab The new tab to observe.
         */
        private void updateObservedTab(Tab newTab) {
            if (mTab != null) mTab.removeObserver(ActivityTabTabObserver.this);
            mTab = newTab;
            if (mTab != null) mTab.addObserver(ActivityTabTabObserver.this);
        }

        /**
         * A notification that the observer has switched to observing a different tab. This can be
         * called a first time with the {@code hint} parameter set to true, indicating that a new
         * tab is going to be selected.
         * @param tab The tab that the observer is now observing. This can be null.
         * @param hint Whether the change event is a hint that a tab change is likely. If true, the
         *             provided tab may still be frozen and is not yet selected.
         */
        protected void onObservingDifferentTab(Tab tab, boolean hint) {}

        /**
         * Clean up any state held by this observer.
         */
        @CallSuper
        public void destroy() {
            if (mTab != null) {
                mTab.removeObserver(this);
                mTab = null;
            }
            removeObserverFromTabProvider();
        }

        @VisibleForTesting
        protected void updateObservedTabToCurrent() {
            updateObservedTab(mTabProvider.get());
        }

        @VisibleForTesting
        protected void addObserverToTabProvider() {
            mTabProvider.addObserver(mActivityTabObserver);
        }

        @VisibleForTesting
        protected void removeObserverFromTabProvider() {
            mTabProvider.removeObserver(mActivityTabObserver);
        }
    }

    /** A handle to the {@link LayoutStateProvider} to get the active layout. */
    private LayoutStateProvider mLayoutStateProvider;

    /** The observer watching scene changes in the active layout. */
    private LayoutStateObserver mLayoutStateObserver;

    /** A handle to the {@link TabModelSelector}. */
    private TabModelSelector mTabModelSelector;

    /** An observer for watching tab creation and switching events. */
    private TabModelSelectorTabModelObserver mTabModelObserver;

    /** An observer for watching tab model switching event. */
    private TabModelSelectorObserver mTabModelSelectorObserver;

    /**
     * Default constructor.
     */
    public ActivityTabProvider() {
        mLayoutStateObserver = new LayoutStateObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {
                if (mTabModelSelector == null) return;
                set(mTabModelSelector.getTabById(tabId));
            }

            @Override
            public void onStartedShowing(@LayoutType int layout, boolean showToolbar) {
                // The {@link SimpleAnimationLayout} is a special case, the intent is not to switch
                // tabs, but to merely run an animation. In this case, do nothing. If the animation
                // layout does result in a new tab {@link TabModelObserver#didSelectTab} will
                // trigger the event instead. If the tab does not change, the event will no
                if (LayoutType.SIMPLE_ANIMATION == layout) return;

                Tab tab = mTabModelSelector.getCurrentTab();
                if (layout != LayoutType.BROWSING) tab = null;
                triggerActivityTabChangeEvent(tab);
            }
        };
    }

    /**
     * @param selector A {@link TabModelSelector} for watching for changes in tabs.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        assert mTabModelSelector == null;
        mTabModelSelector = selector;
        mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                triggerActivityTabChangeEvent(tab);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                // If this is the last tab to close, make sure a signal is sent to the observers.
                if (mTabModelSelector.getCurrentModel().getCount() <= 1) {
                    triggerActivityTabChangeEvent(null);
                }
            }
        };

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                // Send a signal with null tab if a new model has no tab. Other cases
                // are taken care of by TabModelSelectorTabModelObserver#didSelectTab.
                if (newModel.getCount() == 0) triggerActivityTabChangeEvent(null);
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    /**
     * @param layoutStateProvider A {@link LayoutStateProvider} for watching for scene changes.
     */
    public void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert mLayoutStateProvider == null;
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
        // https://crbug.com/1385536 Start surface might be displayed before native is ready.
        if (ChromeFeatureList.sInstantStart.isEnabled()) {
            if (mTabModelSelector == null
                    || !layoutStateProvider.isLayoutVisible(LayoutType.BROWSING)) {
                triggerActivityTabChangeEvent(null);
            }
        }
    }

    /**
     * Check if the interactive tab change event needs to be triggered based on the provided tab.
     * @param tab The activity's tab.
     */
    private void triggerActivityTabChangeEvent(Tab tab) {
        // Allow the event to trigger before native is ready (before the layout manager is set).
        if (mLayoutStateProvider != null
                && !(mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING)
                        || mLayoutStateProvider.isLayoutVisible(LayoutType.SIMPLE_ANIMATION))
                && tab != null) {
            return;
        }

        set(tab);
    }

    /** Clean up and detach any observers this object created. */
    @Override
    public void destroy() {
        if (mLayoutStateProvider != null) mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        mLayoutStateProvider = null;
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        if (mTabModelSelectorObserver != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelectorObserver = null;
        }
        mTabModelSelector = null;
    }
}
