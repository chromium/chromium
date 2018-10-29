// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.SimpleAnimationLayout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

/**
 * A class that provides the current {@link Tab} for various states of the browser's activity.
 */
public class ActivityTabProvider {
    /** An interface to track the visible tab for the activity. */
    public interface ActivityTabObserver {
        /**
         * A notification that the activity's tab has changed. This will be triggered whenever a
         * different tab is selected by the active {@link TabModel} and when that tab is
         * interactive (i.e. not in a tab switching mode). When switching to toolbar swipe or tab
         * switcher, this method will be called with {@code null} to indicate that there is no
         * single activity tab (observers may or may not choose to ignore this event).
         * @param tab The {@link Tab} that became visible or null if not in {@link StaticLayout}.
         * @param hint Whether the change event is a hint that a tab change is likely. If true, the
         *             provided tab may still be frozen and is not yet selected.
         */
        void onActivityTabChanged(Tab tab, boolean hint);
    }

    /** An {@link ActivityTabObserver} that can be used to explicitly watch non-hint events. */
    public static abstract class HintlessActivityTabObserver implements ActivityTabObserver {
        @Override
        public final void onActivityTabChanged(Tab tab, boolean hint) {
            // Only pass the event through if it isn't a hint.
            if (!hint) onActivityTabChanged(tab);
        }

        /**
         * A notification that the {@link Tab} in the {@link StaticLayout} has changed.
         * @param tab The activity's tab.
         */
        public abstract void onActivityTabChanged(Tab tab);
    }

    /** The list of observers to send events to. */
    private final ObserverList<ActivityTabObserver> mObservers = new ObserverList<>();

    /**
     * A single rewindable iterator bound to {@link #mObservers} to prevent constant allocation of
     * new iterators.
     */
    private final RewindableIterator<ActivityTabObserver> mRewindableIterator;

    /** The {@link Tab} that is considered to be the activity's tab. */
    private Tab mActivityTab;

    /** A handle to the {@link LayoutManager} to get the active layout. */
    private LayoutManager mLayoutManager;

    /** The observer watching scene changes in the {@link LayoutManager}. */
    private SceneChangeObserver mSceneChangeObserver;

    /** A handle to the {@link TabModelSelector}. */
    private TabModelSelector mTabModelSelector;

    /** An observer for watching tab creation and switching events. */
    private TabModelSelectorTabModelObserver mTabModelObserver;

    /** The last tab ID that was hinted. This is reset when the activity tab actually changes. */
    private int mLastHintedTabId;

    /**
     * Default constructor.
     */
    public ActivityTabProvider() {
        mRewindableIterator = mObservers.rewindableIterator();
        mSceneChangeObserver = new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {
                if (mTabModelSelector == null || mLastHintedTabId == tabId) return;
                Tab tab = mTabModelSelector.getTabById(tabId);
                mLastHintedTabId = tabId;
                mRewindableIterator.rewind();
                while (mRewindableIterator.hasNext()) {
                    mRewindableIterator.next().onActivityTabChanged(tab, true);
                }
            }

            @Override
            public void onSceneChange(Layout layout) {
                // The {@link SimpleAnimationLayout} is a special case, the intent is not to switch
                // tabs, but to merely run an animation. In this case, do nothing. If the animation
                // layout does result in a new tab {@link TabModelObserver#didSelectTab} will
                // trigger the event instead. If the tab does not change, the event will no
                if (layout instanceof SimpleAnimationLayout) return;

                Tab tab = mTabModelSelector.getCurrentTab();
                if (!(layout instanceof StaticLayout)) tab = null;
                triggerActivityTabChangeEvent(tab);
            }
        };
    }

    /**
     * @return The activity's current tab.
     */
    public Tab getActivityTab() {
        return mActivityTab;
    }

    /**
     * @param selector A {@link TabModelSelector} for watching for changes in tabs.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        assert mTabModelSelector == null;
        mTabModelSelector = selector;
        mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, @TabModel.TabSelectionType int type, int lastId) {
                triggerActivityTabChangeEvent(tab);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                // If this is the last tab to close, make sure a signal is sent to the observers.
                if (mTabModelSelector.getTotalTabCount() <= 1) triggerActivityTabChangeEvent(null);
            }
        };
    }

    /**
     * @param layoutManager A {@link LayoutManager} for watching for scene changes.
     */
    public void setLayoutManager(LayoutManager layoutManager) {
        assert mLayoutManager == null;
        mLayoutManager = layoutManager;
        mLayoutManager.addSceneChangeObserver(mSceneChangeObserver);
    }

    /**
     * Check if the interactive tab change event needs to be triggered based on the provided tab.
     * @param tab The activity's tab.
     */
    private void triggerActivityTabChangeEvent(Tab tab) {
        // Allow the event to trigger before native is ready (before the layout manager is set).
        if (mLayoutManager != null && !(mLayoutManager.getActiveLayout() instanceof StaticLayout)
                && tab != null) {
            return;
        }

        if (mActivityTab == tab) return;
        mActivityTab = tab;
        mLastHintedTabId = Tab.INVALID_TAB_ID;

        mRewindableIterator.rewind();
        while (mRewindableIterator.hasNext()) {
            mRewindableIterator.next().onActivityTabChanged(tab, false);
        }
    }

    /**
     * @param observer The {@link ActivityTabObserver} to add to the activity. This will trigger the
     *                 {@link ActivityTabObserver#onActivityTabChanged(Tab, boolean)} event to be
     *                 called on the added observer, providing access to the current tab.
     */
    public void addObserverAndTrigger(ActivityTabObserver observer) {
        mObservers.addObserver(observer);
        observer.onActivityTabChanged(mActivityTab, false);
    }

    /**
     * @param observer The {@link ActivityTabObserver} to remove from the activity.
     */
    public void removeObserver(ActivityTabObserver observer) {
        mObservers.removeObserver(observer);
    }

    /** Clean up and detach any observers this object created. */
    public void destroy() {
        mObservers.clear();
        if (mLayoutManager != null) mLayoutManager.removeSceneChangeObserver(mSceneChangeObserver);
        mLayoutManager = null;
        if (mTabModelObserver != null) mTabModelObserver.destroy();
        mTabModelSelector = null;
    }
}
