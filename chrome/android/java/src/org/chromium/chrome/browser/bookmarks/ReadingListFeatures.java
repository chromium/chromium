// Copyright 2021 The Chromium Authors. All rights reserved.
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
 * <li>{@code add_to_reading_list_in_app_menu}: boolean; show "Add to Reading List" in the app menu.
 * Default: {@code false}</li>
 * <li>{@code delete_from_reading_list_in_app_menu}: boolean; show "Delete from Reading List" in the
 * app menu. Default: {@code false}</li>
 * <li>{@code edit_reading_list_in_app_menu}: boolean; show "Edit Reading List" in the app menu.
 * Default: {@code false}</li>
 * <li>{@code session_length}: int (seconds); duration Chrome needs to spend in background before it
 * discards the "last bookmark location". Default: {@link #DEFAULT_SESSION_LENGTH_SECONDS}</li>
 * <li>{@code use_root_bookmark_as_default}: boolean; use the root folder rather than "Mobile
 * bookmarks" as the default bookmark folder. Default: {@code false}</li>
 * <li>{@code read_later_min_version}: boolean; see {@link BookmarkFeatures#VERSION}.</li>
 * </ul>
 */
public class ReadingListFeatures {
    /** @see BookmarkFeatures#VERSION */
    static final int VERSION = BookmarkFeatures.VERSION;
    private static final int DEFAULT_SESSION_LENGTH_SECONDS = (int) TimeUnit.HOURS.toSeconds(1);

    private ReadingListFeatures() {}

    public static boolean isReadingListEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.READ_LATER)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                           ChromeFeatureList.READ_LATER, "read_later_min_version", 0)
                <= VERSION;
    }

    /** Returns whether the root folder should be used as the default location. */
    public static boolean shouldUseRootFolderAsDefaultForReadLater() {
        return isReadingListEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.READ_LATER, "use_root_bookmark_as_default", false);
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
    public static boolean isAddToReadingListAppMenuItemEnabled() {
        return isReadingListEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.READ_LATER, "add_to_reading_list_in_app_menu", false);
    }

    /** Returns whether the "Delete from Reading List" app menu item should be enabled. */
    public static boolean isDeleteFromReadingListAppMenuItemEnabled() {
        return isReadingListEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.READ_LATER, "delete_from_reading_list_in_app_menu",
                        false);
    }

    /** Returns whether the "Edit Reading List" app menu item should be enabled. */
    public static boolean isEditReadingListAppMenuItemEnabled() {
        return isReadingListEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.READ_LATER, "edit_reading_list_in_app_menu", false);
    }
}
