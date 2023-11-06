// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;

/** Self-documenting feature class for bookmarks. */
public class BookmarkFeatures {
    private static final MutableFlagWithSafeDefault sAndroidImprovedBookmarksFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS, false);

    /** Returns whether an additional "add bookmark" item should be in the overflow menu. */
    public static boolean isBookmarkMenuItemAsDedicatedRowEnabled() {
        // TODO(wylieb): Remove the BOOKMARKS_REFRESH flag.
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_REFRESH)
                && ShoppingFeatures.isShoppingListEligible();
    }

    /** Returns whether the visual refresh should be used for the bookmark manager. */
    public static boolean isLegacyBookmarksVisualRefreshEnabled() {
        return isBookmarkMenuItemAsDedicatedRowEnabled();
    }

    /**
     * More visual changes to the bookmarks surfaces, with more thumbnails and a focus on search
     * instead of folders/hierarchy.
     */
    public static boolean isAndroidImprovedBookmarksEnabled() {
        return sAndroidImprovedBookmarksFlag.isEnabled();
    }
}
