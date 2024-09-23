// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

/** A class that provides the current {@link Tab} for various states of the browser's activity. */
public class ActivityTabProvider extends ObservableSupplierImpl<Tab> implements Destroyable {
    /**
     * A utility class for observing the activity tab via {@link TabObserver}. When the activity
     * tab changes, the observer is switched to that tab.
     */
    public static class ActivityTabTabObserver extends TabSupplierObserver {
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
            super(tabProvider, shouldTrigger);
        }

        @Override
        protected void onObservingDifferentTab(Tab tab) {
            onObservingDifferentTab(tab, false);
        }

        /**
         * A notification that the observer has switched to observing a different tab. This can be
         * called a first time with the {@code hint} parameter set to true, indicating that a new
         * tab is going to be selected.
         *
         * @param tab The tab that the observer is now observing. This can be null.
         * @param hint Whether the change event is a hint that a tab change is likely. If true, the
         *     provided tab may still be frozen and is not yet selected.
         * @deprecated - hint is unused, override this method without the hint parameter.
         */
        protected void onObservingDifferentTab(Tab tab, boolean hint) {}
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
    private final Callback<TabModel> mCurrentTabModelObserver;

    /** Default constructor. */
    public ActivityTabProvider() {
        mLayoutStateObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layout) {
                        // The {@link SimpleAnimationLayout} is a special case, the intent is not to
                        // switch tabs, but to merely run an animation. In this case, do nothing.
                        // If the animation layout does result in a new tab {@link
                        // TabModelObserver#didSelectTab} will trigger the event instead. If the
                        // tab does not change, the event will noop.
                        if (LayoutType.SIMPLE_ANIMATION == layout) return;

                        Tab tab = mTabModelSelector.getCurrentTab();
                        if (layout != LayoutType.BROWSING) tab = null;
                        triggerActivityTabChangeEvent(tab);
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layout) {
                        if (mTabModelSelector == null) return;

                        if (LayoutType.TAB_SWITCHER == layout) {
                            set(mTabModelSelector.getCurrentTab());
                        }
                    }
                };
        mCurrentTabModelObserver =
                (tabModel) -> {
                    // Send a signal with null tab if a new model has no tab. Other cases
                    // are taken care of by TabModelSelectorTabModelObserver#didSelectTab.
                    if (tabModel.getCount() == 0) triggerActivityTabChangeEvent(null);
                };
    }

    /**
     * @param selector A {@link TabModelSelector} for watching for changes in tabs.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        assert mTabModelSelector == null;
        mTabModelSelector = selector;
        mTabModelObserver =
                new TabModelSelectorTabModelObserver(mTabModelSelector) {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        triggerActivityTabChangeEvent(tab);
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        // If this is the last tab to close, make sure a signal is sent to the
                        // observers.
                        if (mTabModelSelector.getCurrentModel().getCount() <= 1) {
                            triggerActivityTabChangeEvent(null);
                        }
                    }
                };

        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
    }

    /**
     * @param layoutStateProvider A {@link LayoutStateProvider} for watching for scene changes.
     */
    public void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert mLayoutStateProvider == null;
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
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
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
        mTabModelSelector = null;
    }
}
