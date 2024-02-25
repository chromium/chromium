// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

/** Copy of android.provider.Browser.BookmarkColumns. */
public class BookmarkColumns implements BaseColumns {
    /**
     * The URL of the bookmark or history item.
     * <p>Type: TEXT (URL)</p>
     */
    public static final String URL = "url";

    /**
     * The number of time the item has been visited.
     * <p>Type: NUMBER</p>
     */
    public static final String VISITS = "visits";

    /**
     * The date the item was last visited, in milliseconds since the epoch.
     * <p>Type: NUMBER (date in milliseconds since January 1, 1970)</p>
     */
    public static final String DATE = "date";

    /**
     * Flag indicating that an item is a bookmark. A value of 1 indicates a bookmark, a value
     * of 0 indicates a history item.
     * <p>Type: INTEGER (boolean)</p>
     */
    public static final String BOOKMARK = "bookmark";

    /**
     * The user visible title of the bookmark or history item.
     * <p>Type: TEXT</p>
     */
    public static final String TITLE = "title";

    /**
     * The date the item created, in milliseconds since the epoch.
     * <p>Type: NUMBER (date in milliseconds since January 1, 1970)</p>
     */
    public static final String CREATED = "created";

    /**
     * The favicon of the bookmark. Must decode via
     * {@link android.graphics.BitmapFactory#decodeByteArray}.
     * <p>Type: BLOB (image)</p>
     */
    public static final String FAVICON = "favicon";
}
