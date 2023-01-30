// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;

/**
 * Self-documenting feature class for bookmarks.
 */
public class BookmarkFeatures {
    private static final int IMPROVED_SAVE_FLOW_AUTODISMISS_TIME_MS_DEFAULT = 6000;

    private static final MutableFlagWithSafeDefault sAndroidImprovedBookmarksFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS, false);

    /** Returns whether the improved save flow should be shown instead of a snackbar. */
    public static boolean isImprovedSaveFlowEnabled() {
        if (ShoppingFeatures.isShoppingListEligible()) return true;

        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW);
    }

    /** Returns whether the improved save flow should be autodimissed. */
    public static boolean isImprovedSaveFlowAutodismissEnabled() {
        return true;
    }

    /** Returns the time (in ms) that the save flow should wait before dismissing. */
    public static int getImprovedSaveFlowAutodismissTimeMs() {
        return IMPROVED_SAVE_FLOW_AUTODISMISS_TIME_MS_DEFAULT;
    }

    /** Returns whether an additional "add bookmark" item should be in the overflow menu. */
    public static boolean isBookmarkMenuItemAsDedicatedRowEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_REFRESH)
                && ShoppingFeatures.isShoppingListEligible();
    }

    /** Returns whether the visual refresh should be used for the bookmark manager. */
    public static boolean isBookmarksVisualRefreshEnabled() {
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
