// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

/** Copy of android.provider.Browser.SearchColumns. */
public class SearchColumns implements BaseColumns {
    /** The user entered search term. */
    public static final String SEARCH = "search";

    /**
     * The date the search was performed, in milliseconds since the epoch.
     * <p>Type: NUMBER (date in milliseconds since January 1, 1970)</p>
     */
    public static final String DATE = "date";
}
