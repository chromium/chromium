// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.DrawableRes;

import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;

/**
 * A class that holds the data necessary for continuous navigation. Some example data providers
 * include SRP and Discover/Feeds.
 */
public class ContinuousNavigationMetadata {
    /**
     * A class holding information about the provider of a metadata.
     */
    public static class Provider {
        private final @PageCategory int mCategory;
        private final String mName;
        private final @DrawableRes int mIconRes;

        public Provider(@PageCategory int category, String name, @DrawableRes int iconRes) {
            mCategory = category;
            mName = name;
            mIconRes = iconRes;
        }

        int getCategory() {
            return mCategory;
        }

        String getName() {
            return mName;
        }

        @DrawableRes
        int getIconRes() {
            return mIconRes;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof Provider)) {
                return false;
            }
            Provider provider = (Provider) o;
            return mCategory == provider.mCategory && mIconRes == provider.mIconRes
                    && Objects.equals(mName, provider.mName);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mCategory, mName, mIconRes);
        }
    }

    private final GURL mRootUrl;
    private final String mQuery;
    private final Provider mProvider;
    private final List<PageGroup> mGroups;

    ContinuousNavigationMetadata(
            GURL url, String query, Provider provider, List<PageGroup> groups) {
        mRootUrl = url;
        mQuery = query;
        mProvider = provider;
        mGroups = groups;
    }

    GURL getRootUrl() {
        return mRootUrl;
    }

    String getQuery() {
        return mQuery;
    }

    Provider getProvider() {
        return mProvider;
    }

    List<PageGroup> getGroups() {
        return mGroups;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;

        if (!(o instanceof ContinuousNavigationMetadata)) return false;

        ContinuousNavigationMetadata other = (ContinuousNavigationMetadata) o;

        return mRootUrl.equals(other.getRootUrl()) && mQuery.equals(other.getQuery())
                && mProvider.equals(other.getProvider()) && mGroups.equals(other.getGroups());
    }

    @Override
    public int hashCode() {
        return Objects.hash(mRootUrl, mQuery, mProvider, mGroups);
    }
}
