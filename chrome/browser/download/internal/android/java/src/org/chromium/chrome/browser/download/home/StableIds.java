// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

/** Helper class to manage stable ID for items in the recycler view. */
public class StableIds {
    /** The stable ID associated with the storage header. */
    public static long STORAGE_HEADER = Long.MAX_VALUE - 1;

    /** The stable ID associated with the filters row. */
    public static long FILTERS_HEADER = Long.MAX_VALUE - 2;

    /** The stable ID associated with the Just Now section. */
    public static long JUST_NOW_SECTION = Long.MAX_VALUE - 3;

    /** The stable ID associated with the pagination header. */
    public static long PAGINATION_HEADER = Long.MAX_VALUE - 4;
}
