// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.webkit.URLUtil;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * A helper class responsible for determining when to trigger requests for suggestions and when to
 * clear state.
 */
class FetchHelper {
    /** A delegate used to fetch suggestions. */
    interface Delegate {
        /**
         * Request a suggestions fetch for the specified {@code url}.
         * @param url The url for which suggestions should be fetched.
         */
        void requestSuggestions(String url);

        /**
         * Called when the state should be reset e.g. when the user navigates away from a webpage.
         */
        void clearState();

        /**
         * Called when a document becomes eligible for fetching but the fetch is being delayed.
         */
        void reportFetchDelayed(WebContents webContents);
    }

    /** State of the tab with respect to fetching readiness */
    class TabFetchReadinessState {
        private long mFetchTimeBaselineMillis;
        private String mUrl;
        private String mCanonicalUrl;

        TabFetchReadinessState(String url) {
            updateUrl(url);
        }

        /**
         * Updates the URL and if the context is different from the current one, resets the time at
         * which we can start fetching suggestions.
         * @param url A new value for URL tracked by the tab state.
         */
        void updateUrl(String url) {
            mUrl = URLUtil.isNetworkUrl(url) ? url : null;
            mCanonicalUrl = "";
            mFetchTimeBaselineMillis = 0;
        }

        /** @return The current URL tracked by this tab state. */
        String getUrl() {
            return mUrl;
        }

        /** Set the canonical url, which can differ from the actual URL. If the canonical url is
         * set, the readiness state is considered to be tracking both urls. */
        void setCanonicalUrl(String canonicalUrl) {
            mCanonicalUrl = canonicalUrl;
        }

        /**
         * @return Whether the tab state is tracking a tab with valid page loaded and valid status
         *         for fetching.
         */
        boolean isTrackingPage() {
            return mUrl != null;
        }

        /**
         * Sets the baseline from which the fetch delay is calculated if it was not
         * already set (conceptually starting the timer).
         * @param fetchTimeBaselineMillis The new value to set the baseline fetch time to.
         * @return Whether the fetch time baseline was set.
         */
        boolean setFetchTimeBaselineMillis(long fetchTimeBaselineMillis) {
            if (!isTrackingPage()) return false;
            if (isFetchTimeBaselineSet()) return false;
            mFetchTimeBaselineMillis = fetchTimeBaselineMillis;
            return true;
        }

        /** @return The time at which fetch time baseline was established. */
        long getFetchTimeBaselineMillis() {
            return mFetchTimeBaselineMillis;
        }

        /** @return Whether the fetch timer is running. */
        boolean isFetchTimeBaselineSet() {
            return mFetchTimeBaselineMillis != 0;
        }

        /**
         * Checks whether the provided url is the same (ignoring fragments) as the one tracked by
         * tab state.
         * @param url A URL to check against the URL in the tab state.
         * @return Whether the URLs can be considered the same.
         */
        boolean isContextTheSame(String url) {
            return UrlUtilities.urlsMatchIgnoringFragments(url, mUrl)
                    || UrlUtilities.urlsMatchIgnoringFragments(url, mCanonicalUrl);
        }
    }

    @VisibleForTesting
    final static int MINIMUM_FETCH_DELAY_SECONDS = 2; // 2 seconds.
    private final static String FETCH_TRIGGERING_DELAY_SECONDS = "fetch_triggering_delay_seconds";
    private final static String REQUIRE_CURRENT_PAGE_FROM_SRP = "require_current_page_from_SRP";
    private final static String REQUIRE_NAV_CHAIN_FROM_SRP = "require_nav_chain_from_SRP";
    private static boolean sDisableDelayForTesting;
    private static long sFetchTimeBaselineMillisForTesting;

    private final Delegate mDelegate;
    private final TabModelSelector mTabModelSelector;
    private final Map<Integer, TabFetchReadinessState> mObservedTabs = new HashMap<>();

    private TabModelSelectorTabModelObserver mTabModelObserver;
    private TabObserver mTabObserver;
    private boolean mFetchRequestedForCurrentTab;

    private boolean mRequireCurrentPageFromSRP;
    private boolean mRequireNavChainFromSRP;

    @Nullable
    private Tab mCurrentTab;

    /**
     * Construct a new {@link FetchHelper}.
     * @param delegate The {@link Delegate} used to fetch suggestions.
     * @param tabModelSelector The {@link TabModelSelector} for the containing Activity.
     */
    FetchHelper(Delegate delegate, TabModelSelector tabModelSelector) {
        mDelegate = delegate;
        mTabModelSelector = tabModelSelector;

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                assert !tab.isIncognito();
                if (tab == mCurrentTab) {
                    clearState();
                }
                getTabFetchReadinessState(tab).updateUrl(url);
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                assert !tab.isIncognito();
                // This address cases, where pages are implemented as a single page app and
                // switching between articles updates URL, but does not cause a page reload.
                if (tab == mCurrentTab
                        && !getTabFetchReadinessState(tab).isContextTheSame(tab.getUrl())) {
                    clearState();
                    getTabFetchReadinessState(tab).updateUrl(tab.getUrl());
                }
            }

            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                setTimeBaselineAndMaybeFetch(tab);
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                setTimeBaselineAndMaybeFetch(tab);
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                setTimeBaselineAndMaybeFetch(tab);
            }

            private void setTimeBaselineAndMaybeFetch(Tab tab) {
                assert !tab.isIncognito();
                if (getTabFetchReadinessState(tab).setFetchTimeBaselineMillis(
                            SystemClock.uptimeMillis())) {
                    maybeStartFetch(tab);
                }
            }
        };

        mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                startObservingTab(tab);
                if (maybeSetFetchReadinessBaseline(tab)) {
                    maybeStartFetch(tab);
                }
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (tab == null) {
                    if (mCurrentTab != null) clearState();
                    mCurrentTab = null;
                    return;
                }

                if (mCurrentTab != null && mCurrentTab != tab) {
                    clearState();
                }

                if (tab.isIncognito()) {
                    mCurrentTab = null;
                    return;
                }

                // Ensures that we start observing the tab, in case it was added to the tab model
                // before this class.
                startObservingTab(tab);
                mCurrentTab = tab;
                maybeStartFetch(tab);
            }

            @Override
            public void tabRemoved(Tab tab) {
                tabGone(tab);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                tabGone(tab);
            }
        };

        mRequireCurrentPageFromSRP = requireCurrentPageFromSRP();
        mRequireNavChainFromSRP = requireNavChainFromSRP();

        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return;

        mTabModelObserver.didSelectTab(currentTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        if (maybeSetFetchReadinessBaseline(mCurrentTab)) {
            maybeStartFetch(mCurrentTab);
        }
    }

    void destroy() {
        // Remove the observer from all tracked tabs.
        for (Integer tabId : mObservedTabs.keySet()) {
            Tab tab = mTabModelSelector.getTabById(tabId);
            if (tab == null) continue;
            tab.removeObserver(mTabObserver);
        }

        mObservedTabs.clear();
        mTabModelObserver.destroy();
    }

    /**
     * In case the tab is no longer loading the page, it would set the fetch readiness baselines
     * time.
     * @param tab Tab to be checked.
     * @return Whether the baseline time was set.
     */
    private boolean maybeSetFetchReadinessBaseline(final Tab tab) {
        if (isObservingTab(tab) && !tab.isLoading()) {
            return getTabFetchReadinessState(tab).setFetchTimeBaselineMillis(
                    SystemClock.uptimeMillis());
        }
        return false;
    }

    private void maybeStartFetch(final Tab tab) {
        if (tab == null || tab != mCurrentTab) return;

        assert !tab.isIncognito();

        // Skip additional requests for current tab, until clearState is called.
        if (mFetchRequestedForCurrentTab) return;

        TabFetchReadinessState tabFetchReadinessState = getTabFetchReadinessState(mCurrentTab);

        // If we are not tracking a valid page, we can bail.
        if (!tabFetchReadinessState.isTrackingPage()) return;

        // Delay checks and calculations only make sense if the timer is running.
        if (!tabFetchReadinessState.isFetchTimeBaselineSet()) return;

        // Return early if the current page is required to have originated from a Google search
        // but did not.
        if (isFromGoogleSearchRequired() && (tab.getWebContents() == null
                || tab.getWebContents().getNavigationController() == null
                || !isFromGoogleSearch(
                        tab.getWebContents().getNavigationController(),
                        mRequireCurrentPageFromSRP))) {
            return;
        }

        long currentDelayMillis =
                SystemClock.uptimeMillis() - tabFetchReadinessState.getFetchTimeBaselineMillis();
        long delayMillis = Math.max(0, getMinimumFetchDelayMillis() - currentDelayMillis);
        final String url = tabFetchReadinessState.getUrl();

        if (sDisableDelayForTesting || delayMillis == 0) {
            getCanonicalUrlThenFetch(tab, url);
            return;
        }

        mDelegate.reportFetchDelayed(tab.getWebContents());
        ThreadUtils.postOnUiThreadDelayed(() -> getCanonicalUrlThenFetch(tab, url), delayMillis);
    }

    private void getCanonicalUrlThenFetch(final Tab tab, final String url) {
        if (!shouldFetchCanonicalUrl(tab)) {
            fetchSuggestions(tab, url);
            return;
        }

        tab.getWebContents().getMainFrame().getCanonicalUrlForSharing(new Callback<String>() {
            @Override
            public void onResult(String result) {
                if (tab != mCurrentTab) return;

                TabFetchReadinessState tabFetchReadinessState = getTabFetchReadinessState(tab);
                if (tabFetchReadinessState != null && tabFetchReadinessState.isTrackingPage()
                        && tabFetchReadinessState.isContextTheSame(url)) {
                    tabFetchReadinessState.setCanonicalUrl(result);
                    fetchSuggestions(tab, getUrlToFetchFor(tab.getUrl(), result));
                }
            }
        });
    }

    private void fetchSuggestions(final Tab tab, final String url) {
        // Make sure that the tab is currently selected.
        if (tab != mCurrentTab) return;

        if (mFetchRequestedForCurrentTab) return;

        if (!isObservingTab(tab)) return;

        // URL in tab changed since the task was originally posted.
        if (!getTabFetchReadinessState(tab).isContextTheSame(url)) return;

        mFetchRequestedForCurrentTab = true;
        mDelegate.requestSuggestions(url);
    }

    private void clearState() {
        mDelegate.clearState();
        mFetchRequestedForCurrentTab = false;
    }

    /**
     * Starts observing the tab.
     * @param tab The {@link Tab} to be observed.
     */
    private void startObservingTab(Tab tab) {
        if (tab != null && !isObservingTab(tab) && !tab.isIncognito()) {
            mObservedTabs.put(tab.getId(), new TabFetchReadinessState(tab.getUrl()));
            tab.addObserver(mTabObserver);
        }
    }

    /**
     * Stops observing the tab and removes its state.
     * @param tab The {@link Tab} that will no longer be observed.
     */
    private void stopObservingTab(Tab tab) {
        if (tab != null && isObservingTab(tab)) {
            mObservedTabs.remove(tab.getId());
            tab.removeObserver(mTabObserver);
        }
    }

    /**
     * Performs necessary cleanup when a tab leaves the tab model we're associated with, whether by
     * being moved to another model or closed.
     */
    private void tabGone(Tab tab) {
        stopObservingTab(tab);
        if (tab == mCurrentTab) {
            clearState();
            mCurrentTab = null;
        }
    }

    /** Whether the tab is currently observed. */
    @VisibleForTesting
    boolean isObservingTab(Tab tab) {
        return tab != null && mObservedTabs.containsKey(tab.getId());
    }

    private TabFetchReadinessState getTabFetchReadinessState(Tab tab) {
        if (tab == null) return null;
        return mObservedTabs.get(tab.getId());
    }

    /**
     * @param tab The specified {@link Tab}.
     * @return The baseline fetch time for the specified tab.
     */
    long getFetchTimeBaselineMillis(@NonNull Tab tab) {
        return sDisableDelayForTesting
                ? sFetchTimeBaselineMillisForTesting
                : mObservedTabs.get(tab.getId()).getFetchTimeBaselineMillis();
    }

    private boolean isFromGoogleSearchRequired() {
        return mRequireCurrentPageFromSRP || mRequireNavChainFromSRP;
    }

    @VisibleForTesting
    boolean requireCurrentPageFromSRP() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BUTTON, REQUIRE_CURRENT_PAGE_FROM_SRP,
                false);
    }

    @VisibleForTesting
    boolean requireNavChainFromSRP() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BUTTON, REQUIRE_NAV_CHAIN_FROM_SRP, false);
    }

    @VisibleForTesting
    boolean isGoogleSearchUrl(String url) {
        return UrlUtilities.nativeIsGoogleSearchUrl(url);
    }

    @VisibleForTesting
    boolean isFromGoogleSearch(
            NavigationController navController, boolean onlyConsiderPreviousPage) {
        int currentIndex = navController.getLastCommittedEntryIndex();

        // If the current entry is the root of the navigation history, we cannot determine whether
        // it originated from a Google search.
        if (currentIndex <= 0) return false;

        int endIndex = onlyConsiderPreviousPage ? currentIndex - 1 : 0;

        NavigationEntry previousEntry = null;
        NavigationEntry currentEntry;
        for (int i = currentIndex; i > endIndex; i--) {
            if (previousEntry != null) {
                currentEntry = previousEntry;
            } else {
                currentEntry = navController.getEntryAtIndex(i);
            }

            int unmaskedTransition = currentEntry.getTransition() & ~PageTransition.QUALIFIER_MASK;

            // If the current navigation entry was not from one of the accepted transition types
            // return false.
            if (unmaskedTransition != PageTransition.LINK
                    && unmaskedTransition != PageTransition.MANUAL_SUBFRAME
                    && unmaskedTransition != PageTransition.FORM_SUBMIT) {
                return false;
            }

            previousEntry = navController.getEntryAtIndex(i - 1);
            if (isGoogleSearchUrl(previousEntry.getUrl())) {
                return true;
            }
        }

        return false;
    }

    static boolean shouldFetchCanonicalUrl(final Tab currentTab) {
        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) return false;
        if (webContents.getMainFrame() == null) return false;
        String url = currentTab.getUrl();
        if (TextUtils.isEmpty(url)) return false;
        if (currentTab.isShowingErrorPage() || currentTab.isShowingInterstitialPage()
                || SadTab.isShowing(currentTab)) {
            return false;
        }
        return true;
    }

    static String getUrlToFetchFor(String visibleUrl, String canonicalUrl) {
        return TextUtils.isEmpty(canonicalUrl) ? visibleUrl : canonicalUrl;
    }

    @VisibleForTesting
    static long getMinimumFetchDelayMillis() {
        return TimeUnit.SECONDS.toMillis(ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BUTTON, FETCH_TRIGGERING_DELAY_SECONDS,
                MINIMUM_FETCH_DELAY_SECONDS));
    }

    @VisibleForTesting
    static void setDisableDelayForTesting(boolean disable) {
        sDisableDelayForTesting = disable;
    }

    @VisibleForTesting
    static void setFetchTimeBaselineMillisForTesting(long uptimeMillis) {
        sFetchTimeBaselineMillisForTesting = uptimeMillis;
    }
}
