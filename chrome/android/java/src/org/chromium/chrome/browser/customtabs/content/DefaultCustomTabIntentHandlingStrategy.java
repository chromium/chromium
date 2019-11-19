// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.text.TextUtils;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNavigationEventObserver;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;

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
    public void handleInitialIntent(CustomTabIntentDataProvider intentDataProvider) {
        if (mTabProvider.getInitialTabCreationMode() == TabCreationMode.HIDDEN) {
            handleInitialLoadForHiddedTab(intentDataProvider);
        } else {
            LoadUrlParams params = new LoadUrlParams(intentDataProvider.getUrlToLoad());
            mNavigationController.navigate(params, getTimestamp(intentDataProvider));
        }
    }

    // The hidden tab case needs a bit of special treatment.
    private void handleInitialLoadForHiddedTab(CustomTabIntentDataProvider intentDataProvider) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) {
            throw new IllegalStateException("handleInitialIntent called before Tab created");
        }
        String url = intentDataProvider.getUrlToLoad();

        // Manually generating metrics in case the hidden tab has completely finished loading.
        if (!tab.isLoading() && !tab.isShowingErrorPage()) {
            mCustomTabObserver.get().onPageLoadStarted(tab, url);
            mCustomTabObserver.get().onPageLoadFinished(tab, url);
            mNavigationEventObserver.onPageLoadStarted(tab, url);
            mNavigationEventObserver.onPageLoadFinished(tab, url);
        }

        // No actual load to do if the hidden tab already has the exact correct url.
        String speculatedUrl = mTabProvider.getSpeculatedUrl();
        if (TextUtils.equals(speculatedUrl, url)) {
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
    public void handleNewIntent(CustomTabIntentDataProvider intentDataProvider) {
        String url = intentDataProvider.getUrlToLoad();
        if (TextUtils.isEmpty(url)) return;
        LoadUrlParams params = new LoadUrlParams(url);
        mNavigationController.navigate(params, getTimestamp(intentDataProvider));
    }

    private long getTimestamp(CustomTabIntentDataProvider intentDataProvider) {
        return IntentHandler.getTimestampFromIntent(intentDataProvider.getIntent());
    }
}
