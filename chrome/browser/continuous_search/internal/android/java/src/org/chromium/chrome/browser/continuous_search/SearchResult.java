// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.url.GURL;

/**
 * Holds a single result from the SRP.
 */
public class SearchResult {
    private GURL mUrl;
    private String mTitle;

    public SearchResult(GURL url, String title) {
        mUrl = url;
        mTitle = title;
    }

    /**
     * @return url of the result.
     */
    public GURL getUrl() {
        return mUrl;
    }

    /**
     * @return title of the result.
     */
    public String getTitle() {
        return mTitle;
    }
}
