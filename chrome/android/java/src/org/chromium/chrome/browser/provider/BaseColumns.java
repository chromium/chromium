// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

/** Copy of android.provider.BaseColumns. */
public interface BaseColumns {
    /**
     * The unique ID for a row.
     * <P>Type: INTEGER (long)</P>
     */
    public static final String ID = "_id";

    /**
     * The count of rows in a directory.
     * <P>Type: INTEGER</P>
     */
    public static final String COUNT = "_count";
}
