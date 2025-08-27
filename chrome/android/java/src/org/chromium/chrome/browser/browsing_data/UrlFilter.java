// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import org.chromium.build.annotations.NullMarked;

/** A URL->boolean filter used in browsing data deletions. */
@NullMarked
public interface UrlFilter {
    /**
     * @param url The url to be matched.
     * @return Whether this filter matches |url|.
     */
    boolean matchesUrl(String url);
}
