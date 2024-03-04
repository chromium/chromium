// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Self-documenting feature class for bookmarks. */
public class BookmarkFeatures {
    /** Returns whether the visual refresh should be used for the bookmark manager. */
    public static boolean isLegacyBookmarksVisualRefreshEnabled() {
        return FeatureList.isInitialized() && ShoppingFeatures.isShoppingListEligible();
    }

    /**
     * More visual changes to the bookmarks surfaces, with more thumbnails and a focus on search
     * instead of folders/hierarchy.
     */
    public static boolean isAndroidImprovedBookmarksEnabled() {
        return ChromeFeatureList.sAndroidImprovedBookmarks.isEnabled();
    }
}
