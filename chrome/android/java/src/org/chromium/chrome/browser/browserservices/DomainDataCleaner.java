// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.website.SiteDataCleaner;
import org.chromium.chrome.browser.preferences.website.Website;
import org.chromium.chrome.browser.preferences.website.WebsitePermissionsFetcher;
import org.chromium.chrome.browser.util.UrlUtilities;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

/**
 * Implements data clearing for entire domains.
 */
class DomainDataCleaner {
    private final ChromeBrowserInitializer mChromeBrowserInitializer;
    private final SiteDataCleaner mDataCleaner;
    private final WebsitePermissionsFetcher mWebsitePermissionsFetcher;

    DomainDataCleaner(ChromeBrowserInitializer chromeBrowserInitializer,
            SiteDataCleaner dataCleaner, WebsitePermissionsFetcher websitePermissionsFetcher) {
        mChromeBrowserInitializer = chromeBrowserInitializer;
        mDataCleaner = dataCleaner;
        mWebsitePermissionsFetcher = websitePermissionsFetcher;
    }

    /**
     * Clears the data of all {@link Website}s associated with the given domain.
     * Must be called on the main thread.
     * Doesn't require native to be initialized prior to the call.
     */
    void clearData(String domain, Runnable finishCallback) {
        try {
            mChromeBrowserInitializer.handleSynchronousStartup();
        } catch (ProcessInitException e) {
            throw new RuntimeException("Failed to initialize native library", e);
        }

        mWebsitePermissionsFetcher.fetchAllPreferences(
                sites -> clearData(collectSitesOfTargetDomain(sites, domain), finishCallback::run));
    }

    private void clearData(
            Collection<Website> sites, Website.StoredDataClearedCallback finishCallback) {
        clearDataOfCurrentAndScheduleNext(sites.iterator(), finishCallback);
    }

    /**
     * Data cleaning for a collection of {@link Website}s is implemented sequentially to avoid
     * relying on thread-safety of native methods being used.
     * The asynchronous tasks are chained using recursion.
     */
    private void clearDataOfCurrentAndScheduleNext(
            Iterator<Website> siteIterator, Website.StoredDataClearedCallback finishCallback) {
        if (!siteIterator.hasNext()) {
            finishCallback.onStoredDataCleared();
            return;
        }
        mDataCleaner.clearData(siteIterator.next(),
                () -> clearDataOfCurrentAndScheduleNext(siteIterator, finishCallback));
    }

    private Collection<Website> collectSitesOfTargetDomain(
            Collection<Website> sites, String domain) {
        List<Website> sitesToClear = new ArrayList<>();
        for (Website site : sites) {
            String origin = site.getAddress().getOrigin();
            if (UrlUtilities.getDomainAndRegistry(origin, true).equals(domain)) {
                sitesToClear.add(site);
            }
        }
        return sitesToClear;
    }
}
