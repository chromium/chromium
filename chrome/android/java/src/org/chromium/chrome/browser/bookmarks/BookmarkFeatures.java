// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Self-documenting feature class for bookmarks. */
public class BookmarkFeatures {
    /**
     * More visual changes to the bookmarks surfaces, with more thumbnails and a focus on search
     * instead of folders/hierarchy.
     */
    public static boolean isAndroidImprovedBookmarksEnabled() {
        return ChromeFeatureList.sAndroidImprovedBookmarks.isEnabled();
    }
}
