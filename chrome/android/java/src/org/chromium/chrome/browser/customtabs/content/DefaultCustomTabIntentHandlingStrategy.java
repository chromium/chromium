// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabAuthUrlHeuristics;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Default implementation of {@link CustomTabIntentHandlingStrategy}. Navigates the Custom Tab to
 * urls provided in intents.
 */
@NullMarked
public class DefaultCustomTabIntentHandlingStrategy implements CustomTabIntentHandlingStrategy {
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabActivityNavigationController mNavigationController;
    private final CustomTabObserver mCustomTabObserver;
    private final Verifier mVerifier;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final Activity mActivity;

    public DefaultCustomTabIntentHandlingStrategy(
            CustomTabActivityTabProvider tabProvider,
            CustomTabActivityNavigationController navigationController,
            CustomTabObserver customTabObserver,
            Verifier verifier,
            CurrentPageVerifier currentPageVerifier,
            Activity activity) {
        mTabProvider = tabProvider;
        mNavigationController = navigationController;
        mCustomTabObserver = customTabObserver;
        mVerifier = verifier;
        mCurrentPageVerifier = currentPageVerifier;
        mActivity = activity;
    }

    @Override
    public void handleInitialIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        @TabCreationMode int initialTabCreationMode = mTabProvider.getInitialTabCreationMode();
        if (mTabProvider.getTab() != null) {
            CustomTabAuthUrlHeuristics.setFirstCctPageLoadForMetrics(mTabProvider.getTab());
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER)
                && intentDataProvider.isTrustedWebActivity()) {
            Tab tab = mTabProvider.getTab();
            assumeNonNull(tab);
            WebContents webContents = tab.getWebContents();
            assumeNonNull(webContents);
            WebAppLaunchHandler launchHandler =
                    WebAppLaunchHandler.create(
                            mVerifier,
                            mCurrentPageVerifier,
                            mNavigationController,
                            webContents,
                            mActivity);
            launchHandler.handleInitialIntent(intentDataProvider);
        }

        if (initialTabCreationMode == TabCreationMode.HIDDEN) {
            handleInitialLoadForHiddenTab(intentDataProvider);
        } else {
            assumeNonNull(intentDataProvider.getUrlToLoad());
            LoadUrlParams params = new LoadUrlParams(intentDataProvider.getUrlToLoad());
            mNavigationController.navigate(params, assumeNonNull(intentDataProvider.getIntent()));
        }

        CustomTabAuthUrlHeuristics.recordUrlParamsHistogram(intentDataProvider.getUrlToLoad());
        CustomTabAuthUrlHeuristics.recordRedirectUriSchemeHistogram(intentDataProvider);
    }

    // The hidden tab case needs a bit of special treatment.
    private void handleInitialLoadForHiddenTab(
            BrowserServicesIntentDataProvider intentDataProvider) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) {
            throw new IllegalStateException("handleInitialIntent called before Tab created");
        }
        String url = assertNonNull(intentDataProvider.getUrlToLoad());

        // No actual load to do if the hidden tab already has the exact correct url.
        String speculatedUrl = mTabProvider.getSpeculatedUrl();

        boolean useSpeculation = TextUtils.equals(speculatedUrl, url);
        boolean hasCommitted = !assumeNonNull(tab.getWebContents()).getLastCommittedUrl().isEmpty();
        mCustomTabObserver.trackNextPageLoadForHiddenTab(
                tab.getWebContents(),
                useSpeculation,
                hasCommitted,
                assumeNonNull(intentDataProvider.getIntent()));

        if (useSpeculation) return;

        LoadUrlParams params = new LoadUrlParams(url);

        // The following block is a hack that deals with urls preloaded with
        // the wrong fragment. Does an extra pageload and replaces history.
        if (speculatedUrl != null && UrlUtilities.urlsFragmentsDiffer(speculatedUrl, url)) {
            params.setShouldReplaceCurrentEntry(true);
        }

        mNavigationController.navigate(params, intentDataProvider.getIntent());
    }

    private void loadUrl(BrowserServicesIntentDataProvider intentDataProvider) {
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

        mNavigationController.navigate(params, assumeNonNull(intentDataProvider.getIntent()));
    }

    @Override
    public void handleNewIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER)
                && intentDataProvider.isTrustedWebActivity()) {
            Tab tab = mTabProvider.getTab();
            assumeNonNull(tab);
            WebContents webContents = tab.getWebContents();
            assumeNonNull(webContents);
            WebAppLaunchHandler launchHandler =
                    WebAppLaunchHandler.create(
                            mVerifier,
                            mCurrentPageVerifier,
                            mNavigationController,
                            webContents,
                            mActivity);
            launchHandler.handleNewIntent(intentDataProvider);
        } else {
            loadUrl(intentDataProvider);
        }
    }
}
