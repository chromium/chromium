// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * A tab observer for watching for SRPs to read data from.
 */
public class ContinuousSearchTabObserver extends EmptyTabObserver implements SearchResultListener {
    @VisibleForTesting
    Boolean mOriginMatchesForTesting;

    private Tab mTab;
    private SearchResultProducer mProducer;

    public ContinuousSearchTabObserver(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
    }

    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
        if (!navigation.hasCommitted() || !navigation.isInPrimaryMainFrame()
                || navigation.isSameDocument()) {
            return;
        }

        ContinuousNavigationUserDataImpl continuousNavigationUserData =
                ContinuousNavigationUserDataImpl.getOrCreateForTab(tab);
        if (!continuousNavigationUserData.isValid()) return;

        TraceEvent.begin("ContinuousSearchTabObserver#onDidFinishNavigation");
        GURL originalUrl = null;
        if (tab.getWebContents() != null) {
            originalUrl = SearchUrlHelper.getOriginalUrlFromWebContents(tab.getWebContents());
        }
        // If a redirect occurs from an expected link to a page with the same origin allow the UI to
        // still show.
        GURL currentUrl = currentUrl(originalUrl, navigation.getUrl());

        continuousNavigationUserData.updateCurrentUrl(currentUrl);
        TraceEvent.end("ContinuousSearchTabObserver#onDidFinishNavigation");
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        ContinuousNavigationUserDataImpl continuousNavigationUserData =
                ContinuousNavigationUserDataImpl.getOrCreateForTab(tab);
        if (ContinuousSearchConfiguration.isPermanentlyDismissed()) {
            continuousNavigationUserData.invalidateData();
            return;
        }

        // Cancel any existing requests.
        resetProducer();

        // Don't fetch new data if we already have data for this SRP.
        if (continuousNavigationUserData.isMatchingSrp(url)) return;

        String query = SearchUrlHelper.getQueryIfValidSrpUrl(url);
        if (query == null) return;

        mProducer = SearchResultProducerFactory.create(tab, this);

        // TODO: Remove this once mProducer is always created.
        if (mProducer == null) return;

        mProducer.fetchResults(url, query);
    }

    @Override
    public void onCloseContents(Tab tab) {
        resetProducer();
        ContinuousNavigationUserDataImpl.getOrCreateForTab(tab).invalidateData();
    }

    @Override
    public void onDestroyed(Tab tab) {
        // If the tab is destroyed the {@link UserDataHost} will also be destroyed. We need to stop
        // {@link #onResult()} from running by resetting the producer and cancelling the request.
        resetProducer();

        // The tab's {@link UserDataHost} is destroyed after running observers so this is safe.
        ContinuousNavigationUserDataImpl.getOrCreateForTab(tab).invalidateData();

        tab.removeObserver(this);
    }

    // SearchResultListener

    @Override
    public void onResult(ContinuousNavigationMetadata metadata) {
        assert metadata != null;

        if (mProducer == null) return;

        TraceEvent.begin("ContinuousSearchTabObserver#onResult");
        mProducer = null;

        ContinuousNavigationUserDataImpl.getOrCreateForTab(mTab).updateData(
                metadata, mTab.getUrl());
        TraceEvent.end("ContinuousSearchTabObserver#onResult");
    }

    @Override
    public void onError(int errorCode) {
        // TODO: Handle errors.
        mProducer = null;
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    private GURL currentUrl(@Nullable GURL originalUrl, GURL committedUrl) {
        if (GURL.isEmptyOrInvalid(originalUrl)) return committedUrl;

        return originMatches(originalUrl, committedUrl) ? originalUrl : committedUrl;
    }

    private boolean originMatches(GURL originalUrl, GURL committedUrl) {
        if (mOriginMatchesForTesting != null) {
            return mOriginMatchesForTesting.booleanValue();
        }
        return originalUrl.getOrigin().equals(committedUrl.getOrigin());
    }

    private void resetProducer() {
        if (mProducer != null) {
            mProducer.cancel();
            mProducer = null;
        }
    }
}
