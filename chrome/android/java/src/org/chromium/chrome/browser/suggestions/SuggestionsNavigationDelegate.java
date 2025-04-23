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
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/** Extension of {@link NativePageNavigationDelegate} with suggestions-specific methods. */
public class SuggestionsNavigationDelegate extends NativePageNavigationDelegateImpl {

    public SuggestionsNavigationDelegate(
            Activity activity,
            Profile profile,
            NativePageHost host,
            TabModelSelector tabModelSelector,
            Tab tab) {
        super(activity, profile, host, tabModelSelector, tab);
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
            assert windowOpenDisposition != WindowOpenDisposition.NEW_WINDOW
                    : "Tabs in groups cannot be opened in a new window.";
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

        boolean laxSchemeHost = ChromeFeatureList.sMostVisitedTilesReselectLaxSchemeHost.getValue();
        boolean laxRef = ChromeFeatureList.sMostVisitedTilesReselectLaxRef.getValue();
        boolean laxQuery = ChromeFeatureList.sMostVisitedTilesReselectLaxQuery.getValue();
        boolean laxPath = ChromeFeatureList.sMostVisitedTilesReselectLaxPath.getValue();
        UrlSimilarityScorer scorer =
                new UrlSimilarityScorer(keyUrl, laxSchemeHost, laxRef, laxQuery, laxPath);
        MatchResult result = scorer.findTabWithMostSimilarUrl(tabModel);
        scorer.recordMatchResult(result);
        if (result.index == TabList.INVALID_TAB_INDEX) return false;

        tabModel.setIndex(result.index, TabSelectionType.FROM_USER);
        tabModel.getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(mTab).allowUndo(false).build(),
                        /* allowDialog= */ false);
        return true;
    }
}
