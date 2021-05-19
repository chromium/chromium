// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.continuous_search.SearchResultExtractorClientStatus;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * A tab observer for watching for SRPs to read data from.
 */
public class ContinuousSearchTabObserver extends EmptyTabObserver implements SearchResultListener {
    private Tab mTab;
    private SearchResultProducer mProducer;

    public ContinuousSearchTabObserver(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        ContinuousNavigationUserDataImpl continuousNavigationUserData =
                ContinuousNavigationUserDataImpl.getOrCreateForTab(tab);
        continuousNavigationUserData.updateCurrentUrl(url);
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        ContinuousNavigationUserDataImpl continuousNavigationUserData =
                ContinuousNavigationUserDataImpl.getOrCreateForTab(tab);
        continuousNavigationUserData.updateCurrentUrl(url);

        // Cancel any existing requests.
        resetProducer();

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
        tab.removeObserver(this);
    }

    // SearchResultListener

    @Override
    public void onResult(ContinuousNavigationMetadata metadata) {
        assert metadata != null;
        reportStatus(mProducer.getSuccessStatus(), mProducer.getClass());
        mProducer = null;

        ContinuousNavigationUserDataImpl.getOrCreateForTab(mTab).updateData(
                metadata, mTab.getUrl());
    }

    @Override
    public void onError(int errorCode) {
        // TODO: Handle errors.
        reportStatus(errorCode, mProducer.getClass());
        mProducer = null;
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    private void resetProducer() {
        if (mProducer != null) {
            mProducer.cancel();
            mProducer = null;
        }
    }

    private void reportStatus(int status, Class<?> clazz) {
        if (clazz == SearchResultExtractorProducer.class) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Browser.ContinuousSearch.SearchResultExtractionStatus", status,
                    SearchResultExtractorClientStatus.MAX_VALUE);
        }
    }
}
