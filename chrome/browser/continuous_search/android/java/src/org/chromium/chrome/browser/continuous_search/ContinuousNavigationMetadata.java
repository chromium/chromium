// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;

/**
 * A class that holds the data necessary for continuous navigation.
 * Some example data providers include SRP and Discover/Feeds.
 */
public class ContinuousNavigationMetadata {
    private final GURL mRootUrl;
    private final String mQuery;
    private final @PageCategory int mCategory;
    private final List<PageGroup> mGroups;

    ContinuousNavigationMetadata(
            GURL url, String query, @PageCategory int category, List<PageGroup> groups) {
        mRootUrl = url;
        mQuery = query;
        mCategory = category;
        mGroups = groups;
    }

    GURL getRootUrl() {
        return mRootUrl;
    }

    String getQuery() {
        return mQuery;
    }

    int getCategory() {
        return mCategory;
    }

    List<PageGroup> getGroups() {
        return mGroups;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;

        if (!(o instanceof ContinuousNavigationMetadata)) return false;

        ContinuousNavigationMetadata other = (ContinuousNavigationMetadata) o;
        return mRootUrl.equals(other.mRootUrl) && mQuery.equals(other.mQuery)
                && mCategory == other.mCategory && mGroups.equals(other.mGroups);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mRootUrl, mQuery, mCategory, mGroups);
    }
}
