// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import androidx.annotation.NonNull;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class HistoryClustersToolbarProperties {
    static class QueryState {
        private final String mQuery;
        private final boolean mIsSearching;

        static QueryState forQuery(@NonNull String query) {
            return new QueryState(query, true);
        }

        static QueryState forQueryless() {
            return new QueryState(null, false);
        }

        private QueryState(String query, boolean isSearching) {
            mQuery = query;
            mIsSearching = isSearching;
        }

        String getQuery() {
            return mQuery;
        }

        boolean isSearching() {
            return mIsSearching;
        }
    }

    static final WritableObjectPropertyKey<QueryState> QUERY_STATE =
            new WritableObjectPropertyKey<>("query state");

    static final PropertyKey[] ALL_KEYS = {QUERY_STATE};
}
