// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.url.GURL;

import java.util.List;

/**
 * A class that holds the extracted data from a SRP.
 */
public class SearchResultMetadata {
    private final GURL mResultUrl;
    private final String mQuery;
    private final int mCategory;
    private final List<SearchResultGroup> mGroups;

    SearchResultMetadata(GURL url, String query, int category, List<SearchResultGroup> groups) {
        mResultUrl = url;
        mQuery = query;
        mCategory = category;
        mGroups = groups;
    }

    GURL getResultUrl() {
        return mResultUrl;
    }

    String getQuery() {
        return mQuery;
    }

    int getCategory() {
        return mCategory;
    }

    List<SearchResultGroup> getGroups() {
        return mGroups;
    }
}
