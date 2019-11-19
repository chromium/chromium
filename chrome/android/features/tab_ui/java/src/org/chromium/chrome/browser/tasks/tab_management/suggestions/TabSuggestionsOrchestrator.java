// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

/**
 * Represents the entry point for the TabSuggestions component. Responsible for
 * registering and invoking the different {@link TabSuggestionsFetcher}.
 */
public class TabSuggestionsOrchestrator implements TabSuggestions, Destroyable {
    public static final String TAB_SUGGESTIONS_UMA_PREFIX = "TabSuggestionsOrchestrator";
    private static final String TAG = "TabSuggestionsDetailed";
    private static final int MIN_CLOSE_SUGGESTIONS_THRESHOLD = 3;

    protected TabContextObserver mTabContextObserver;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private List<TabSuggestionsFetcher> mTabSuggestionsFetchers;
    private List<TabSuggestion> mPrefetchedResults = new LinkedList<>();
    private TabContext mPrefetchedTabContext;
    private TabModelSelector mTabModelSelector;
    private ObserverList<TabSuggestionsObserver> mTabSuggestionsObservers;
    private int mRemainingFetchers;

    public TabSuggestionsOrchestrator(
            TabModelSelector selector, ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mTabModelSelector = selector;
        mTabSuggestionsFetchers = new LinkedList<>();
        mTabSuggestionsFetchers.add(new TabSuggestionsClientFetcher());
        mTabSuggestionsObservers = new ObserverList<>();
        mTabContextObserver = new TabContextObserver(selector) {
            @Override
            public void onTabContextChanged(@TabContextChangeReason int changeReason) {
                synchronized (mPrefetchedResults) {
                    if (mPrefetchedTabContext != null) {
                        for (TabSuggestionsObserver tabSuggestionsObserver :
                                mTabSuggestionsObservers) {
                            tabSuggestionsObserver.onTabSuggestionInvalidated();
                        }
                    }
                }
                prefetchSuggestions();
            }
        };
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        activityLifecycleDispatcher.register(this);
    }

    private List<TabSuggestion> aggregateResults(List<TabSuggestion> tabSuggestions) {
        List<TabSuggestion> aggregated = new LinkedList<>();
        for (TabSuggestion tabSuggestion : tabSuggestions) {
            switch (tabSuggestion.getAction()) {
                case TabSuggestion.TabSuggestionAction.CLOSE:
                    if (tabSuggestion.getTabsInfo().size() >= MIN_CLOSE_SUGGESTIONS_THRESHOLD) {
                        aggregated.add(tabSuggestion);
                    }
                    break;
                case TabSuggestion.TabSuggestionAction.GROUP:
                    if (!tabSuggestion.getTabsInfo().isEmpty()) {
                        aggregated.add(tabSuggestion);
                    }
                    break;
                default:
                    android.util.Log.e(
                            TAG, String.format("Unknown action: %d", tabSuggestion.getAction()));
                    break;
            }
        }
        Collections.shuffle(aggregated);
        return aggregated;
    }

    @Override
    public void destroy() {
        mTabContextObserver.destroy();
        mActivityLifecycleDispatcher.unregister(this);
    }

    /**
     * Acquire suggestions and store so suggestions are available for the UI
     * thread on demand.
     */
    protected void prefetchSuggestions() {
        TabContext tabContext = TabContext.createCurrentContext(mTabModelSelector);
        synchronized (mPrefetchedResults) {
            mRemainingFetchers = 0;
            mPrefetchedTabContext = tabContext;
            mPrefetchedResults = new LinkedList<>();
            for (TabSuggestionsFetcher tabSuggestionsFetcher : mTabSuggestionsFetchers) {
                if (tabSuggestionsFetcher.isEnabled()) {
                    mRemainingFetchers++;
                    tabSuggestionsFetcher.fetch(tabContext, res -> prefetchCallback(res));
                }
            }
        }
    }

    private void prefetchCallback(TabSuggestionsFetcherResults suggestions) {
        synchronized (mPrefetchedResults) {
            // If the tab context has changed since the fetchers were used,
            // we simply ignore the result as it is no longer relevant.
            if (suggestions.tabContext.equals(mPrefetchedTabContext)) {
                mRemainingFetchers--;
                mPrefetchedResults.addAll(suggestions.tabSuggestions);
                if (mRemainingFetchers == 0) {
                    for (TabSuggestionsObserver tabSuggestionsObserver : mTabSuggestionsObservers) {
                        tabSuggestionsObserver.onNewSuggestion(
                                aggregateResults(mPrefetchedResults));
                    }
                }
            }
        }
    }

    @Override
    public void addObserver(TabSuggestionsObserver tabSuggestionsObserver) {
        mTabSuggestionsObservers.addObserver(tabSuggestionsObserver);
    }

    @Override
    public void removeObserver(TabSuggestionsObserver tabSuggestionsObserver) {
        mTabSuggestionsObservers.removeObserver(tabSuggestionsObserver);
    }
}
