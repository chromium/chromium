// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.app.Activity;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.UrlSimilarityScorer.MatchResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

/** Extension of {@link NativePageNavigationDelegate} with suggestions-specific methods. */
public class SuggestionsNavigationDelegate extends NativePageNavigationDelegateImpl {

    private static final String MOST_VISITED_TILES_RESELECT_LAX_PATH_PARAM = "lax_path";
    public static final BooleanCachedFieldTrialParameter MOST_VISITED_TILES_RESELECT_LAX_PATH =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.MOST_VISITED_TILES_RESELECT,
                    MOST_VISITED_TILES_RESELECT_LAX_PATH_PARAM,
                    false);
    private static final String MOST_VISITED_TILES_RESELECT_LAX_QUERY_PARAM = "lax_query";
    public static final BooleanCachedFieldTrialParameter MOST_VISITED_TILES_RESELECT_LAX_QUERY =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.MOST_VISITED_TILES_RESELECT,
                    MOST_VISITED_TILES_RESELECT_LAX_QUERY_PARAM,
                    false);
    private static final String MOST_VISITED_TILES_RESELECT_LAX_REF_PARAM = "lax_ref";
    public static final BooleanCachedFieldTrialParameter MOST_VISITED_TILES_RESELECT_LAX_REF =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.MOST_VISITED_TILES_RESELECT,
                    MOST_VISITED_TILES_RESELECT_LAX_REF_PARAM,
                    false);
    private static final String MOST_VISITED_TILES_RESELECT_LAX_SCHEME_HOST_PARAM =
            "lax_scheme_host";
    public static final BooleanCachedFieldTrialParameter
            MOST_VISITED_TILES_RESELECT_LAX_SCHEME_HOST =
                    ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                            ChromeFeatureList.MOST_VISITED_TILES_RESELECT,
                            MOST_VISITED_TILES_RESELECT_LAX_SCHEME_HOST_PARAM,
                            false);

    public SuggestionsNavigationDelegate(
            Activity activity,
            Profile profile,
            NativePageHost host,
            TabModelSelector tabModelSelector,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            Tab tab) {
        super(activity, profile, host, tabModelSelector, tabGroupCreationDialogManager, tab);
    }

    /**
     * Opens the suggestions page without recording metrics.
     *
     * @param windowOpenDisposition How to open (new window, current tab, etc).
     * @param url The url to navigate to.
     * @param inGroup Whether the navigation is in a group.
     */
    public void navigateToSuggestionUrl(int windowOpenDisposition, String url, boolean inGroup) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        if (inGroup) {
            openUrlInGroup(windowOpenDisposition, loadUrlParams);
        } else {
            openUrl(windowOpenDisposition, loadUrlParams);
        }
    }

    /**
     * Searches for a tab whose URL matches the specified URL. If found, selects the first (by
     * tabId) matching tab, closes `mTab` (assumed to be the NTP), and returns true. Otherwise does
     * nothing and returns false.
     *
     * @param keyUrl The URL to search for.
     */
    public boolean maybeSelectTabWithUrl(GURL keyUrl) {
        TabModel tabModel = mTabModelSelector.getModel(/* incognito= */ false);

        boolean laxSchemeHost = MOST_VISITED_TILES_RESELECT_LAX_SCHEME_HOST.getValue();
        boolean laxRef = MOST_VISITED_TILES_RESELECT_LAX_REF.getValue();
        boolean laxQuery = MOST_VISITED_TILES_RESELECT_LAX_QUERY.getValue();
        boolean laxPath = MOST_VISITED_TILES_RESELECT_LAX_PATH.getValue();
        UrlSimilarityScorer scorer =
                new UrlSimilarityScorer(keyUrl, laxSchemeHost, laxRef, laxQuery, laxPath);
        MatchResult result = scorer.findTabWithMostSimilarUrl(tabModel);
        scorer.recordMatchResult(result);
        if (result.index == TabList.INVALID_TAB_INDEX) return false;

        tabModel.setIndex(result.index, TabSelectionType.FROM_USER);
        tabModel.closeTabs(TabClosureParams.closeTab(mTab).allowUndo(false).build());
        return true;
    }
}
