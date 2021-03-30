// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.text.TextUtils;

import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNavigationEventObserver;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Default implementation of {@link CustomTabIntentHandlingStrategy}.
 * Navigates the Custom Tab to urls provided in intents.
 */
@ActivityScope
public class DefaultCustomTabIntentHandlingStrategy implements CustomTabIntentHandlingStrategy {
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabActivityNavigationController mNavigationController;
    private final CustomTabNavigationEventObserver mNavigationEventObserver;
    private final Lazy<CustomTabObserver> mCustomTabObserver;

    @Inject
    public DefaultCustomTabIntentHandlingStrategy(CustomTabActivityTabProvider tabProvider,
            CustomTabActivityNavigationController navigationController,
            CustomTabNavigationEventObserver navigationEventObserver,
            Lazy<CustomTabObserver> customTabObserver) {
        mTabProvider = tabProvider;
        mNavigationController = navigationController;
        mNavigationEventObserver = navigationEventObserver;
        mCustomTabObserver = customTabObserver;
    }

    @Override
    public void handleInitialIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        @TabCreationMode
        int initialTabCreationMode = mTabProvider.getInitialTabCreationMode();
        if (initialTabCreationMode == TabCreationMode.HIDDEN
                || initialTabCreationMode == TabCreationMode.FROM_STARTUP_TAB_PRELOADER) {
            handleInitialLoadForHiddedTab(initialTabCreationMode, intentDataProvider);
        } else {
            LoadUrlParams params = new LoadUrlParams(intentDataProvider.getUrlToLoad());
            mNavigationController.navigate(params, getTimestamp(intentDataProvider));
        }
    }

    @RemovableInRelease
    // TODO(yfriedman): Remove & inline once CustomTabs junit tests can be created from a provided
    // GURL. This depends on switching *IntentDataProvider over to GURL.
    // https://crbug.com/783819
    public GURL getGurlForUrl(String url) {
        // TODO(mthiesse): As this is user-provided it should be going through UrlFormatter.fixupUrl
        return new GURL(url);
    }

    // The hidden tab case needs a bit of special treatment.
    private void handleInitialLoadForHiddedTab(@TabCreationMode int initialTabCreationMode,
            BrowserServicesIntentDataProvider intentDataProvider) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) {
            throw new IllegalStateException("handleInitialIntent called before Tab created");
        }
        String url = intentDataProvider.getUrlToLoad();
        GURL gurl = getGurlForUrl(url);

        // Manually generating metrics in case the hidden tab has completely finished loading.
        if (!tab.isLoading() && !tab.isShowingErrorPage()) {
            mCustomTabObserver.get().onPageLoadStarted(tab, gurl);
            mCustomTabObserver.get().onPageLoadFinished(tab, gurl);
            mNavigationEventObserver.onPageLoadStarted(tab, gurl);
            mNavigationEventObserver.onPageLoadFinished(tab, gurl);
        }

        // No actual load to do if the hidden tab already has the exact correct url.
        String speculatedUrl = mTabProvider.getSpeculatedUrl();
        if (TextUtils.equals(speculatedUrl, url)
                || initialTabCreationMode == TabCreationMode.FROM_STARTUP_TAB_PRELOADER) {
            // In the TabCreationMode.FROM_STARTUP_TAB_PRELOADER case:
            // - CustomActivityTabProvider#getSpeculatedUrl() is not set.
            // - The tab creation mode is only set in CustomTabActivityTabController if the URL
            // being loaded is the one we want.

            if (tab.isLoading()) {
                // CustomTabObserver and CustomTabActivityNavigationObserver are attached
                // as observers in CustomTabActivityTabController, not when the navigation is
                // initiated in HiddenTabHolder or StartupTabPreloader.
                mCustomTabObserver.get().onPageLoadStarted(tab, gurl);
                mNavigationEventObserver.onPageLoadStarted(tab, gurl);
            }
            return;
        }

        LoadUrlParams params = new LoadUrlParams(url);

        // The following block is a hack that deals with urls preloaded with
        // the wrong fragment. Does an extra pageload and replaces history.
        if (speculatedUrl != null
                && UrlUtilities.urlsFragmentsDiffer(speculatedUrl, url)) {
            params.setShouldReplaceCurrentEntry(true);
        }

        mNavigationController.navigate(params, getTimestamp(intentDataProvider));
    }

    @Override
    public void handleNewIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        String url = intentDataProvider.getUrlToLoad();
        if (TextUtils.isEmpty(url)) return;
        LoadUrlParams params = new LoadUrlParams(url);

        if (intentDataProvider.isWebApkActivity()) {
            // The back stack should be cleared when a WebAPK receives a deep link intent. This is
            // unnecessary for Trusted Web Activities and new-style WebAPKs because Trusted Web
            // Activities and new-style WebAPKs are restarted when they receive an intent from a
            // deep link.
            params.setShouldClearHistoryList(true);
        }

        mNavigationController.navigate(params, getTimestamp(intentDataProvider));
    }

    private long getTimestamp(BrowserServicesIntentDataProvider intentDataProvider) {
        return IntentHandler.getTimestampFromIntent(intentDataProvider.getIntent());
    }
}
