// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.text.TextUtils;

/** Implementations of URLFilter used in tests. */
public final class UrlFilters {
    /** A trivial implementation of {@link UrlFilter} that matches all urls. */
    public static class AllUrls implements UrlFilter {
        @Override
        public boolean matchesUrl(String url) {
            return true;
        }
    }

    /** A trivial implementation of {@link UrlFilter} that matches a single url. */
    public static class OneUrl implements UrlFilter {
        private final String mUrl;

        /** @param url The single url to be matched by this filter. */
        public OneUrl(String url) {
            mUrl = url;
        }

        @Override
        public boolean matchesUrl(String url) {
            return TextUtils.equals(url, mUrl);
        }
    }

    private UrlFilters() {}
}
