// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/**
 * Reading List feature flags and params.
 *
 * <p>{@link ChromeFeatureList#READ_LATER}
 * <ul>
 * <li>{@code reading_list_in_app_menu}: boolean; show Add/Delete menu items in the app menu.
 * Default: {@code false}</li>
 * <li>{@code session_length}: int (seconds); duration Chrome needs to spend in background before it
 * discards the "last bookmark location". Default: {@link #DEFAULT_SESSION_LENGTH_SECONDS}</li>
 * <li>{@code use_cct}: boolean; open Reading list items in CCT. Default: {@code true}</li>
 * <li>{@code use_root_bookmark_as_default}: boolean; use the root folder rather than "Mobile
 * bookmarks" as the default bookmark folder. Default: {@code true}</li>
 * <li>{@code read_later_min_version}: boolean; see {@link BookmarkFeatures#VERSION}.</li>
 * <li>{@code allow_bookmark_type_swapping}: boolean; Allow type swapping between bookmarks and
 * reading list types. Default: {@code false}</li>
 * </ul>
 */
public class ReadingListFeatures {
    /** @see BookmarkFeatures#VERSION */
    static final int VERSION = BookmarkFeatures.VERSION;
    private static final int DEFAULT_SESSION_LENGTH_SECONDS = (int) TimeUnit.HOURS.toSeconds(1);

    private static Boolean sShouldUseCustomTabForTesting;

    private ReadingListFeatures() {}

    /** Returns whether Reading list items should open in a custom tab. */
    public static boolean shouldUseCustomTab() {
        if (sShouldUseCustomTabForTesting != null) return sShouldUseCustomTabForTesting;

        if (!isReadingListEnabled()) return true;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.READ_LATER, "use_cct", true);
    }

    /** Returns whether the root folder should be used as the default location. */
    public static boolean shouldUseRootFolderAsDefaultForReadLater() {
        return isReadingListEnabled();
    }

    /**
     * Returns the duration Chrome needs to spend in background before it discards the "last
     * bookmark location".
     */
    public static int getSessionLengthMs() {
        return (int) TimeUnit.SECONDS.toMillis(ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.READ_LATER, "session_length", DEFAULT_SESSION_LENGTH_SECONDS));
    }

    /** Returns whether the "Add to Reading List" app menu item should be enabled. */
    public static boolean isReadingListMenuItemAsDedicatedRowEnabled() {
        return isReadingListEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.READ_LATER, "reading_list_in_app_menu", false);
    }

    public static boolean shouldAllowBookmarkTypeSwapping() {
        return isReadingListEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.READ_LATER, "allow_bookmark_type_swapping", false);
    }

    private static boolean isReadingListEnabled() {
        // This feature is enabled by default in native.
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.READ_LATER)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                           ChromeFeatureList.READ_LATER, "read_later_min_version", 0)
                <= VERSION;
    }

    public static void setShouldUseCustomTabForTesting(boolean enabled) {
        sShouldUseCustomTabForTesting = enabled;
    }
}
