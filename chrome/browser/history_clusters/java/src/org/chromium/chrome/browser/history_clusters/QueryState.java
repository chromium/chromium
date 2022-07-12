// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import androidx.annotation.NonNull;

/**
 * Class representing the state of the search UI. There are two meaningful properties: whether a
 * search is currently active and what that search is. Making these properties distinct lets us
 * distinguish between a state where a search is active but no text is entered and one where no
 * search is active.
 */
public class QueryState {
    private final String mQuery;
    private final String mSearchEmptyString;
    private final boolean mIsSearching;

    public static QueryState forQuery(@NonNull String query, @NonNull String searchEmptyString) {
        return new QueryState(query, searchEmptyString, true);
    }

    public static QueryState forQueryless() {
        return new QueryState("", null, false);
    }

    private QueryState(String query, String searchEmptyString, boolean isSearching) {
        mQuery = query;
        mSearchEmptyString = searchEmptyString;
        mIsSearching = isSearching;
    }

    String getQuery() {
        return mQuery;
    }

    String getSearchEmptyString() {
        return mSearchEmptyString;
    }

    boolean isSearching() {
        return mIsSearching;
    }
}