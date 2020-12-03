// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * API for implementations that produce search result data.
 */
public abstract class SearchResultProducer {
    protected final Tab mTab;
    protected final SearchResultListener mListener;

    SearchResultProducer(Tab tab, SearchResultListener listener) {
        mTab = tab;
        mListener = listener;
    }

    /**
     * Asynchronously fetches results. Results are returned via {@link mListener}.
     * @param url The url that data should be retrieved for.
     */
    abstract void fetchResults(GURL url, String query);

    /**
     * Cancels the request.
     */
    abstract void cancel();
}
