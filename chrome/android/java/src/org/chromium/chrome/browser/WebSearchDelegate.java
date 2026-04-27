// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.enterprise.util.DataProtectionBridge;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/** Delegate to execute a web search based on a query. */
@NullMarked
public class WebSearchDelegate {
    private final ActivityTabProvider mActivityTabProvider;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    public WebSearchDelegate(
            ActivityTabProvider activityTabProvider,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mActivityTabProvider = activityTabProvider;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
    }

    /**
     * Executes the web search.
     *
     * @param searchText The text to be searched.
     */
    public void performSearch(String searchText) {
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        if (tabModelSelector == null) return;

        String query =
                ActionModeCallbackHelper.sanitizeQuery(
                        searchText, ActionModeCallbackHelper.MAX_SEARCH_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        Tab tab = assumeNonNull(mActivityTabProvider.get());

        DataProtectionBridge.shouldAllowSearchWith(
                searchText.length(),
                tab.getWebContents(),
                () -> {
                    if (tab.isDestroyed()) return;
                    Profile profile = tab.getProfile();
                    TrackerFactory.getTrackerForProfile(profile)
                            .notifyEvent(EventConstants.WEB_SEARCH_PERFORMED);
                    tabModelSelector.openNewTab(
                            generateUrlParamsForSearch(profile, query),
                            TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                            tab,
                            tab.isIncognito());
                });
    }

    /** Generate the LoadUrlParams necessary to load the specified search query. */
    private static LoadUrlParams generateUrlParamsForSearch(Profile profile, String query) {
        TemplateUrlService service = TemplateUrlServiceFactory.getForProfile(profile);
        String url = service.getUrlForSearchQuery(query);
        String headers = GeolocationHeader.getGeoHeader(url, profile, service);

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setVerbatimHeaders(headers);
        loadUrlParams.setTransitionType(PageTransition.GENERATED);
        return loadUrlParams;
    }
}
