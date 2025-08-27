// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import org.chromium.build.annotations.NullMarked;

/** Copy of android.provider.BaseColumns. */
@NullMarked
public interface BaseColumns {
    /**
     * The unique ID for a row.
     *
     * <p>Type: INTEGER (long)
     */
    String ID = "_id";

    /**
     * The count of rows in a directory.
     *
     * <p>Type: INTEGER
     */
    String COUNT = "_count";
}
