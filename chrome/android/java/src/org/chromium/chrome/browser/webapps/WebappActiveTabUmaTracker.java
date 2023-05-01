// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;

/**
 * Handles recording user metrics for active tab.
 */
public class WebappActiveTabUmaTracker extends ActivityTabTabObserver {
    @VisibleForTesting
    public static final String HISTOGRAM_NAVIGATION_STATUS = "Webapp.NavigationStatus";

    private BrowserServicesIntentDataProvider mIntentDataProvider;
    private CurrentPageVerifier mCurrentPageVerifier;

    public WebappActiveTabUmaTracker(ActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            CurrentPageVerifier currentPageVerifier) {
        super(tabProvider);
        mIntentDataProvider = intentDataProvider;
        mCurrentPageVerifier = currentPageVerifier;
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        if (navigation.hasCommitted() && !navigation.isSameDocument()) {
            RecordHistogram.recordBooleanHistogram(
                    HISTOGRAM_NAVIGATION_STATUS, !navigation.isErrorPage());
        }
    }
}
