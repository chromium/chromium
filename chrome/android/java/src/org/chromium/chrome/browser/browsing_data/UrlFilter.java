// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

/** A URL->boolean filter used in browsing data deletions. */
public interface UrlFilter {
    /**
     * @param url The url to be matched.
     * @return Whether this filter matches |url|.
     */
    public boolean matchesUrl(String url);
}
